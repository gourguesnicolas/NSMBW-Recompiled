namespace Translator.Core.Parsing.Rel;

/// <summary>A REL module and the guest address its image is placed at.</summary>
public sealed record RelModulePlacement(RelFile File, uint BaseAddress);
