KDbg Release Notes for version 3.2.0
====================================

These are the notable changes since version 3.1.0.

Changes
-------

- KDbg can be built for KDE Frameworks 6/Qt6 as well as
  KDE Frameworks 5/Qt5. Use cmake variable BUILD_FOR_KDE_VERSION to
  request the version (see also the README file for instructions).
  Thanks to Sebastian Pipping, Andreas Sturmlechner, Dāvis Mosāns for
  their help and contributions.

- XSL debugging support was removed.

Bug Fixes
---------

- The value parser is now more careful when the name of an operator
  function is part of a value, and now knows about operator<=>.

- Modern GDBs are much more talkative on start-up and actually
  successful start-ups were diagnosed as failures, so that debugging
  sessions would not even be possible. This is now fixed.
