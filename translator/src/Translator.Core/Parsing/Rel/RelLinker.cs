using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Translator.Core.Loading;

namespace Translator.Core.Parsing.Rel;

/// <summary>
/// Relocates a set of REL modules that import from each other (for example several game modules
/// that call into one another) and returns one image per module, in input order.
/// </summary>
public static class RelLinker
{
    public static IReadOnlyList<RelImage> Link(
        IReadOnlyList<RelModulePlacement> modules,
        uint dolBaseAddress = MemoryLayout.DolBaseAddress)
    {
        ArgumentNullException.ThrowIfNull(modules);

        var byId = new Dictionary<uint, RelModulePlacement>();
        foreach (var module in modules)
        {
            var id = module.File.ModuleId;
            if (id == 0)
            {
                throw new InvalidDataException("REL module id 0 is reserved for the main executable");
            }

            if (!byId.TryAdd(id, module))
            {
                throw new InvalidDataException($"REL module id {id} appears more than once in the module set");
            }
        }

        var ordered = modules.OrderBy(m => m.BaseAddress).ToList();
        for (var i = 1; i < ordered.Count; i++)
        {
            var previous = ordered[i - 1];
            var previousEnd = checked(previous.BaseAddress + previous.File.TotalSize);
            if (ordered[i].BaseAddress < previousEnd)
            {
                throw new InvalidDataException(
                    $"REL modules {previous.File.ModuleId} (0x{previous.BaseAddress:X8}-0x{previousEnd:X8}) and " +
                    $"{ordered[i].File.ModuleId} (from 0x{ordered[i].BaseAddress:X8}) overlap");
            }
        }

        foreach (var module in modules)
        {
            foreach (var imported in module.File.ImportedModuleIds)
            {
                if (imported != 0 && !byId.ContainsKey(imported))
                {
                    throw new InvalidDataException(
                        $"REL module {module.File.ModuleId} imports module {imported}, which is not part of the module set");
                }
            }
        }

        return modules
            .Select(m => m.File.BuildLinkedImage(m.BaseAddress, byId, dolBaseAddress))
            .ToList();
    }
}
