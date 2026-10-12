using System;
using System.IO;
using Translator.Cli.Configuration;

namespace Translator.Tests;

public sealed class ProjectRelModulesConfigTests
{
    private const string Header = """
        schema_version: 1
        workspace_root: .
        project:
          id: rel-modules-config-test
        inputs:
          dol:
            path: main.dol
        """;

    [Fact]
    public void ParsesSeveralModulesWithOptionalBssAddress()
    {
        var temp = CreateTempDirectory();
        try
        {
            File.WriteAllBytes(Path.Combine(temp, "main.dol"), new byte[0x100]);
            File.WriteAllBytes(Path.Combine(temp, "a.rel"), new byte[0x10]);
            File.WriteAllBytes(Path.Combine(temp, "b.rel"), new byte[0x10]);
            var projectPath = Path.Combine(temp, "recomp.yml");
            File.WriteAllText(projectPath, Header + Environment.NewLine + """
                  rels:
                    - path: a.rel
                      load_address: 0x80768500
                      bss_address: 0x8076D4A0
                    - path: b.rel
                      load_address: 0x8076D6C0
                """);

            var project = TranslationProjectConfig.Load(projectPath);

            Assert.Null(project.Inputs.Rel);
            Assert.Equal(2, project.Inputs.Rels.Count);
            Assert.Equal(Path.Combine(temp, "a.rel"), project.Inputs.Rels[0].Path);
            Assert.Equal(0x80768500u, project.Inputs.Rels[0].LoadAddress);
            Assert.Equal(0x8076D4A0u, project.Inputs.Rels[0].BssAddress);
            Assert.Equal(0x8076D6C0u, project.Inputs.Rels[1].LoadAddress);
            Assert.Null(project.Inputs.Rels[1].BssAddress);
        }
        finally
        {
            Directory.Delete(temp, recursive: true);
        }
    }

    [Fact]
    public void RejectsRelAndRelsTogether()
    {
        var temp = CreateTempDirectory();
        try
        {
            File.WriteAllBytes(Path.Combine(temp, "main.dol"), new byte[0x100]);
            File.WriteAllBytes(Path.Combine(temp, "a.rel"), new byte[0x10]);
            var projectPath = Path.Combine(temp, "recomp.yml");
            File.WriteAllText(projectPath, Header + Environment.NewLine + """
                  rel:
                    path: a.rel
                    load_address: 0x80768500
                  rels:
                    - path: a.rel
                      load_address: 0x80768500
                """);

            var error = Assert.Throws<InvalidDataException>(() => TranslationProjectConfig.Load(projectPath));
            Assert.Contains("inputs.rels", error.Message, StringComparison.Ordinal);
        }
        finally
        {
            Directory.Delete(temp, recursive: true);
        }
    }

    private static string CreateTempDirectory()
    {
        var path = Path.Combine(Path.GetTempPath(), "rel-modules-config-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(path);
        return path;
    }
}
