using System;
using System.Linq;
using Translator.Core.Parsing.Dol;
using Translator.Core.Parsing.Rel;

namespace Translator.Core.Loading;

public sealed class ProgramImageBuilder
{
    public ProgramImage Build(string dolPath, string relPath, uint relBaseAddress)
    {
        var dol = DolFile.Load(dolPath);
        var rel = RelFile.Load(relPath).BuildImage(relBaseAddress);
        return Build(dol, rel);
    }

    public ProgramImage Build(
        DolFile dol,
        RelImage? rel = null,
        uint ramBase = MemoryLayout.RamBase,
        int ramSize = MemoryLayout.RamSize)
    {
        if (ramSize <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(ramSize));
        }

        var ram = new byte[ramSize];

        // Some DOL headers report a coarse BSS span that overlaps initialized small-data sections.
        // Clear first so the bytes that exist in the file win when ranges overlap.
        foreach (var bss in dol.Sections.Where(s => s.Kind == SectionKind.Bss))
        {
            var offset = checked((int)(bss.VirtualAddress - ramBase));
            var end = offset + (int)bss.Size;
            if (offset < 0 || end > ram.Length)
            {
                throw new InvalidOperationException($"BSS section spills outside RAM window (offset {offset}, end {end})");
            }

            ram.AsSpan(offset, (int)bss.Size).Clear();
        }

        foreach (var section in dol.Sections)
        {
            if (section.Size == 0 || section.Kind == SectionKind.Bss)
            {
                continue;
            }

            var offset = checked((int)(section.VirtualAddress - ramBase));
            var end = offset + (int)section.Size;
            if (offset < 0 || end > ram.Length)
            {
                throw new InvalidOperationException($"DOL section {section.Name} spills outside RAM window (offset {offset}, end {end})");
            }

            section.Data.Span.CopyTo(ram.AsSpan(offset, (int)section.Size));
        }

        var relRange = new AddressRange(0, 0);
        if (rel is not null)
        {
            CopyRel(ram, rel, ramBase);
            relRange = rel.Range;
        }

        var usedRange = rel is null ? dol.MemoryRange : AddressRange.Union(dol.MemoryRange, rel.Range);

        var sha = ChecksumUtilities.Sha256Hex(ram);
        return new ProgramImage(ram, usedRange, dol.MemoryRange, relRange, sha, ramBase);
    }

    /// <summary>
    /// Builds an image from a DOL and several REL module images, typically the output of
    /// <see cref="RelLinker.Link"/>. Modules must not overlap each other or the DOL.
    /// </summary>
    public ProgramImage BuildWithModules(
        DolFile dol,
        IReadOnlyList<RelImage> rels,
        uint ramBase = MemoryLayout.RamBase,
        int ramSize = MemoryLayout.RamSize)
    {
        ArgumentNullException.ThrowIfNull(rels);
        if (rels.Count == 0)
        {
            return Build(dol, rel: null, ramBase, ramSize);
        }

        if (rels.Count == 1)
        {
            return Build(dol, rels[0], ramBase, ramSize);
        }

        foreach (var rel in rels)
        {
            if (Overlaps(rel.Range, dol.MemoryRange))
            {
                throw new InvalidOperationException(
                    $"REL module {rel.ModuleId} at 0x{rel.Range.Start:X8}-0x{rel.Range.End:X8} overlaps the DOL " +
                    $"(0x{dol.MemoryRange.Start:X8}-0x{dol.MemoryRange.End:X8})");
            }
        }

        for (var i = 0; i < rels.Count; i++)
        {
            for (var j = i + 1; j < rels.Count; j++)
            {
                if (Overlaps(rels[i].Range, rels[j].Range))
                {
                    throw new InvalidOperationException(
                        $"REL modules {rels[i].ModuleId} and {rels[j].ModuleId} overlap");
                }
            }
        }

        var baseImage = Build(dol, rel: null, ramBase, ramSize);
        var ram = baseImage.Memory;
        var span = rels[0].Range;
        var usedRange = dol.MemoryRange;
        foreach (var rel in rels)
        {
            CopyRel(ram, rel, ramBase);
            span = AddressRange.Union(span, rel.Range);
            usedRange = AddressRange.Union(usedRange, rel.Range);
        }

        var sha = ChecksumUtilities.Sha256Hex(ram);
        return new ProgramImage(
            ram,
            usedRange,
            dol.MemoryRange,
            span,
            sha,
            ramBase,
            rels.Select(r => r.Range).ToArray());
    }

    private static void CopyRel(byte[] ram, RelImage rel, uint ramBase)
    {
        var relOffset = checked((int)(rel.BaseAddress - ramBase));
        var relEnd = relOffset + rel.Data.Length;
        if (relOffset < 0 || relEnd > ram.Length)
        {
            throw new InvalidOperationException($"REL image spills outside RAM window (offset {relOffset}, end {relEnd})");
        }

        rel.Data.CopyTo(ram, relOffset);
    }

    private static bool Overlaps(AddressRange a, AddressRange b) => a.Start < b.End && b.Start < a.End;
}
