"""Check public pointer-created identities across fresh Runtime processes."""

from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def main() -> None:
    identities: set[str] = set()
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="axiom-object-identity-") as directory:
        for process in range(3):
            report = Path(directory) / f"process-{process}.xml"
            result = subprocess.run(
                [str(executable),
                 "--gtest_filter=*IndependentWritersMergeWithoutRewritingIdentity*",
                 f"--gtest_output=xml:{report}"],
                capture_output=True, text=True, timeout=60,
            )
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            properties = ET.parse(report).findall(".//property[@name='first_object_id']")
            if len(properties) != 7:
                raise AssertionError(f"Expected seven pointer tools, got {len(properties)}")
            for property in properties:
                identity = property.attrib["value"]
                if len(identity) != 32 or int(identity, 16) == 0 or identity in identities:
                    raise AssertionError(f"Invalid or reused cross-process ObjectId: {identity}")
                identities.add(identity)
    print(f"Verified {len(identities)} distinct ObjectIds across three fresh processes")


if __name__ == "__main__":
    main()
