using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Translator.Core.Loading;
using Translator.Core.Parsing.Dol;
using Translator.Core.Parsing.Rel;

namespace Translator.Tests;

/// <summary>
/// Multi-module REL linking on synthetic modules (no game data). Each synthetic module has one
/// 8-byte data section (section 1); its relocations patch the two words of that section.
/// </summary>
public class RelLinkerTests
{
    private const byte RAddr32 = 1;
    private const byte RRel24 = 10;
    private const byte RRvlSect = 202;
    private const byte RRvlStop = 203;

    private const uint DataSectionFileOffset = 0x80;

    private sealed record Reloc(uint Offset, byte Type, byte SymbolSection, uint Addend);

    /// <summary>Builds a version 3 REL with sections [null, data(8 bytes)] and the given imports.</summary>
    private static byte[] BuildRel(uint moduleId, params (uint ModuleId, Reloc[] Relocs)[] imports)
    {
        const int headerSize = 0x4C;
        const int sectionCount = 2;
        var sectionTable = headerSize;
        var dataEnd = (int)DataSectionFileOffset + 8;
        var importTable = dataEnd;
        var relocTable = importTable + imports.Length * 8;

        var relocStreams = new List<byte[]>();
        foreach (var (_, relocs) in imports)
        {
            var entries = new List<byte>();
            void Entry(ushort delta, byte type, byte section, uint addend)
            {
                var e = new byte[8];
                BinaryPrimitives.WriteUInt16BigEndian(e.AsSpan(0), delta);
                e[2] = type;
                e[3] = section;
                BinaryPrimitives.WriteUInt32BigEndian(e.AsSpan(4), addend);
                entries.AddRange(e);
            }

            Entry(0, RRvlSect, 1, 0);
            uint cursor = 0;
            foreach (var r in relocs.OrderBy(r => r.Offset))
            {
                Entry((ushort)(r.Offset - cursor), r.Type, r.SymbolSection, r.Addend);
                cursor = r.Offset;
            }

            Entry(0, RRvlStop, 0, 0);
            relocStreams.Add(entries.ToArray());
        }

        var size = relocTable + relocStreams.Sum(s => s.Length);
        var raw = new byte[size];
        void W32(int offset, uint value) => BinaryPrimitives.WriteUInt32BigEndian(raw.AsSpan(offset), value);

        W32(0x00, moduleId);
        W32(0x0C, sectionCount);
        W32(0x10, (uint)sectionTable);
        W32(0x1C, 3);                       // version
        W32(0x20, 0);                       // bss size
        W32(0x24, (uint)relocTable);
        W32(0x28, (uint)importTable);
        W32(0x2C, (uint)(imports.Length * 8));
        W32(0x40, 8);                       // align
        W32(0x44, 8);                       // bss align
        W32(0x48, (uint)importTable);       // fix size

        W32(sectionTable + 8, DataSectionFileOffset); // section 1: data, not executable
        W32(sectionTable + 12, 8);

        var relocCursor = relocTable;
        for (var i = 0; i < imports.Length; i++)
        {
            W32(importTable + i * 8, imports[i].ModuleId);
            W32(importTable + i * 8 + 4, (uint)relocCursor);
            relocStreams[i].CopyTo(raw, relocCursor);
            relocCursor += relocStreams[i].Length;
        }

        return raw;
    }

    private static uint Word(RelImage image, int index) =>
        BinaryPrimitives.ReadUInt32BigEndian(image.Data.AsSpan((int)DataSectionFileOffset + index * 4));

    private static RelFile ModuleA() => RelFile.FromBytes(BuildRel(
        moduleId: 1,
        (2, new[] { new Reloc(0, RAddr32, 1, 4) }),
        (0, new[] { new Reloc(4, RAddr32, 0, 0x80001234) })));

    private static RelFile ModuleB() => RelFile.FromBytes(BuildRel(moduleId: 2));

    [Fact]
    public void ReadsModuleIdAndImports()
    {
        var a = ModuleA();

        Assert.Equal(1u, a.ModuleId);
        Assert.Equal(new uint[] { 2, 0 }, a.ImportedModuleIds);
        Assert.True(a.TotalSize >= DataSectionFileOffset + 8);
    }

    [Fact]
    public void ResolvesImportsAgainstOtherModules()
    {
        var images = RelLinker.Link(new[]
        {
            new RelModulePlacement(ModuleA(), 0x80500000),
            new RelModulePlacement(ModuleB(), 0x80510000),
        });

        Assert.Equal(2, images.Count);
        Assert.Equal(1u, images[0].ModuleId);
        Assert.Equal(0x80510000u + DataSectionFileOffset + 4, Word(images[0], 0)); // module 2, section 1 + 4
        Assert.Equal(0x80001234u, Word(images[0], 1));                              // absolute DOL address
    }

    [Fact]
    public void ResolvesCrossModuleBranches()
    {
        // bl from module 1 (data word 0, used as an instruction slot) to module 2, section 1 + 0
        var raw = BuildRel(moduleId: 1, (2, new[] { new Reloc(0, RRel24, 1, 0) }));
        BinaryPrimitives.WriteUInt32BigEndian(raw.AsSpan((int)DataSectionFileOffset), 0x48000001); // bl +0
        var images = RelLinker.Link(new[]
        {
            new RelModulePlacement(RelFile.FromBytes(raw), 0x80500000),
            new RelModulePlacement(ModuleB(), 0x80510000),
        });

        var source = 0x80500000u + DataSectionFileOffset;
        var target = 0x80510000u + DataSectionFileOffset;
        Assert.Equal(0x48000001u | ((target - source) & 0x03FFFFFC), Word(images[0], 0));
    }

    [Fact]
    public void SingleRelBuildKeepsResolvingNonDolImportsAgainstItself()
    {
        // The pre-existing single-REL path is unchanged: without a module set, every non-DOL import
        // resolves against the module being built.
        var image = ModuleA().BuildImage(0x80500000);

        Assert.Equal(0x80500000u + DataSectionFileOffset + 4, Word(image, 0));
        Assert.Equal(0x80001234u, Word(image, 1));
    }

    [Fact]
    public void RejectsMissingImportedModule()
    {
        var ex = Assert.Throws<InvalidDataException>(() =>
            RelLinker.Link(new[] { new RelModulePlacement(ModuleA(), 0x80500000) }));
        Assert.Contains("imports module 2", ex.Message);
    }

    [Fact]
    public void RejectsDuplicateModuleIds()
    {
        Assert.Throws<InvalidDataException>(() => RelLinker.Link(new[]
        {
            new RelModulePlacement(ModuleB(), 0x80500000),
            new RelModulePlacement(ModuleB(), 0x80510000),
        }));
    }

    [Fact]
    public void RejectsOverlappingModules()
    {
        Assert.Throws<InvalidDataException>(() => RelLinker.Link(new[]
        {
            new RelModulePlacement(ModuleA(), 0x80500000),
            new RelModulePlacement(ModuleB(), 0x80500010),
        }));
    }

    [Fact]
    public void ProgramImageHoldsEveryModule()
    {
        var dol = BuildDol(entry: 0x80004000, textAddress: 0x80004000, textSize: 0x20);
        var images = RelLinker.Link(new[]
        {
            new RelModulePlacement(ModuleA(), 0x80500000),
            new RelModulePlacement(ModuleB(), 0x80510000),
        });

        var program = new ProgramImageBuilder().BuildWithModules(dol, images);

        Assert.True(program.HasRel);
        Assert.Equal(new[] { images[0].Range, images[1].Range }, program.RelRanges);
        Assert.Equal(0x80500000u, program.RelRange.Start);
        Assert.Equal(images[1].Range.End, program.RelRange.End);
        var word = BinaryPrimitives.ReadUInt32BigEndian(
            program.Memory.AsSpan(program.GetOffset(0x80500000u + DataSectionFileOffset, 4)));
        Assert.Equal(0x80510000u + DataSectionFileOffset + 4, word);
    }

    [Fact]
    public void SingleRelProgramImageReportsOneRange()
    {
        var dol = BuildDol(entry: 0x80004000, textAddress: 0x80004000, textSize: 0x20);
        var rel = ModuleB().BuildImage(0x80510000);

        var program = new ProgramImageBuilder().Build(dol, rel);

        Assert.Equal(new[] { rel.Range }, program.RelRanges);
    }

    private static DolFile BuildDol(uint entry, uint textAddress, uint textSize)
    {
        var raw = new byte[0x100 + textSize];
        BinaryPrimitives.WriteUInt32BigEndian(raw.AsSpan(0x00), 0x100);       // text0 file offset
        BinaryPrimitives.WriteUInt32BigEndian(raw.AsSpan(0x48), textAddress); // text0 address
        BinaryPrimitives.WriteUInt32BigEndian(raw.AsSpan(0x90), textSize);    // text0 size
        BinaryPrimitives.WriteUInt32BigEndian(raw.AsSpan(0xE0), entry);
        using var stream = new MemoryStream(raw, writable: false);
        return DolFile.Load(stream);
    }
}
