Graphics samples for [john-chapman.github.io](https://john-chapman.github.io/).

Use `git clone --recursive` to init/clone all submodules, as follows:

```
git clone --recursive https://github.com/john-chapman/GfxSamples.git
```

_[Git LFS](https://git-lfs.github.com/) is required; install before cloning._

Build via build/premake.lua as follows, requires [premake5](https://premake.github.io/):

```
premake5 --file=premake.lua [target]
```

### Dependencies

Submodule dependencies:
- [ApplicationTools](https://github.com/john-chapman/ApplicationTools)
- [GfxSampleFramework](https://github.com/john-chapman/GfxSampleFramework)
