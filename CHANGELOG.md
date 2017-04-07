# Change log

This file documents all notable changes to Coral.  This includes major
new features, important bug fixes and breaking changes.  For a more
detailed list of all changes, click the header links for each version.
This will take you to the relevant sections of the project's
[Git commit history](https://github.com/viproma/coral).

Coral version numbers follow the [Semantic Versioning](http://semver.org/)
scheme.  As long as we're still in the initial (pre-1.0) development stage,
all new versions must be expected to contain backwards-incompatible changes.
We don't guarantee that they'll all be documented here, but we'll try to
list the bigger ones.

## [Unreleased]

## [0.8.0] – 2017-04-07
### Added
  - Support for FMI 2.0.
  - A `--realtime` switch to enable soft real time synchronisation in
    coralmaster.
  - A `--debug-pause` switch which causes coralmaster to stop at the
    beginning of the simulation, before any FMI function calls, to allow
    time to attach a debugger to the slaves.
  - This CHANGELOG file.
### Changed
  - A negative number may now be given for any communication timeout,
    which will disable the timeout entirely.
  - The default network interface in coralmaster and coralslaveprovider
    has been changed from `*` to `127.0.0.1`.
    (See issue [#20](https://github.com/viproma/coral/issues/20).)

## [0.7.1] – 2017-03-09
### Fixed
  - Issue [#9](https://github.com/viproma/coral/issues/9):
    `coral/fmi/importer.hpp` depends on private header

## 0.7.0 – 2017-02-07
First public release.

[Unreleased]: https://github.com/viproma/coral/compare/v0.8.0...master
[0.8.0]: https://github.com/viproma/coral/compare/v0.7.1...v0.8.0
[0.7.1]: https://github.com/viproma/coral/compare/v0.7.0...v0.7.1
