# FC04 MSE — progress 005 (not a Gate result)

MSE-18 now runs each of the five actual CTest fixtures separately: single-object selected actions, Section consumer, Image facade including Crop, Connector facade, and Connector drag preview. The build target requires their real executables. Each child CTest uses an exact anchored name and `--no-tests=error`; missing tests and failing processes cannot be counted as success. The first attempt exposed Windows `cmd.exe` quoting and failed; correcting its outer quote pair yielded exit 0.

A full `ctest --test-dir <absolute build> -R ^FC04[.]MSE- --output-on-failure --no-tests=error --output-junit <absolute mse-after-regression-binding.xml>` run exited 8: 22 test names passed and MSE-15, 20, 22, 23 and 26 failed because their GTest cases are missing. `raw.zip` contains the exact full JUnit and log. Passing a test name does not imply its every frozen variant or physical platform has been covered.

The prior progress 004 evidence and all existing P31 inputs remain immutable. The independent D07 allocation sweep hang remains open, not rerun.
