"""Copy Linux shared libraries/plugins from the resolved host graph only."""

from pathlib import Path

from conan.tools.files import copy


def deploy(graph, output_folder, **kwargs):
    conanfile = graph.root.conanfile
    destination = str(Path(output_folder) / "lib")
    for dependency in conanfile.dependencies.host.values():
        if dependency.package_folder:
            # Keep subdirectories such as MariaDB lib/plugin and OpenSSL ossl-modules.
            copy(conanfile, "*.so*", src=str(Path(dependency.package_folder) / "lib"),
                 dst=destination, keep_path=True)
