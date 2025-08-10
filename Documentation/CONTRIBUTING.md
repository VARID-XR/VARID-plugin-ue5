# Contributing

If you want to contribute to VARID, please email: <hello@varid.com>

## Code of Conduct
This project and everyone participating in it is governed by the Code of Conduct. By participating, you are expected to uphold this code. Please report unacceptable behavior to the VARID Consortium.

## Bugs & Enhancements
- Use Github built in issue tracking
- https://github.com/VARID-XR/VARID-plugin-ue5/issues

## Pull Requests
- Pull requests are required.
- Commit message format: should have at least type e.g. add, fix and some context e.g. rendering, profile, FX.

## General Coding Styles
- https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine

## Naming Conventions
- https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects

## Specific Coding Styles
- VARID Prefix
  - Prefix classes/structs/files/etc with the VARID name.
  - It's short, unique and makes it easy to identify & namespace all VARID code.
- Shaders
  - Within C++, prefer calling the parameter object 'PassParameters'. This is inline with Unreal Engine code.
- Alphabetical ordering
  - Where applicable, always work with VARID concepts in a consistent alphabetical order. e.g. for FX
    - The render method builds up FX VF Maps in alphabetical order.
- Function parameters
  - All input parameters prefixed with In.
  - All output parameter prefixed with Out.
  - Prefixes make it very clear what the scope of the variable belongs to.