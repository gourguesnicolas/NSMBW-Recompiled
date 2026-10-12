using Translator.Core.Loading;

namespace Translator.Core.Parsing.Rel;

public sealed class RelImage
{
    public RelImage(byte[] data, uint baseAddress, uint moduleId = 0)
    {
        Data = data;
        BaseAddress = baseAddress;
        ModuleId = moduleId;
        Range = AddressRange.FromStartAndSize(baseAddress, (uint)data.Length);
    }

    public byte[] Data { get; }
    public uint BaseAddress { get; }

    /// <summary>Module id from the REL header, or 0 when the image was not built from a REL file.</summary>
    public uint ModuleId { get; }

    public AddressRange Range { get; }
}
