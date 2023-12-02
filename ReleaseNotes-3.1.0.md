KDbg Release Notes for version 3.1.0
====================================

These are the notable changes since 3.0.1.

Enhancements
------------

- The disassembly flavor to be used for the display of disassembled code
  can be selected. The setting is per program.
  Thanks to Petros Siligkounas.

- The break ("pulse") button now shows help text.

Bug fixes
---------

- Icons of certain actions were missing, which has been fixed, most
  importantly of the Open Executable action, thanks to  Sebastian Pipping.

- The value parser no longer chokes on references to incomplete types.

Development Support
-------------------

- Github action integration was added, thanks to Sebastian Pipping.

- Debian build procedure was modernized, thanks to Sebastian Pipping.

- Many modernizations took place so that the build procedure now
  reports far fewer deprecation warnings.

Deprecation notice
------------------

- XSL debugging support is no longer maintained and is now deprecated.
  It will be removed in a future release.
