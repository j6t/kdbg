## Updating a "XX.po" file

To replace translation strings in an existing "XX.po" file to improve
the translation, just edit the file.  Please use tools like Poedit or
KBabel, because they update the meta-data correctly.

To find new translatable strings in source files and propagate them
to "XX.po", run this command:

```shell
./preparemessages.sh XX.po
```

It will:

- Generate a new "kdbg.pot" file
- Call "msgmerge --add-location --backup=off -U XX.po git.pot"
  to update "XX.po"

The "--add-location" option for msgmerge will add location lines,
and these location lines will help translation tools to locate
translation context easily.


## Preparing a "XX.po" file for commit

To save a location-less "po/XX.po" automatically in the repository,
define the driver for the "gettext-no-location" clean filter like this:

```shell
git config filter.gettext-no-location.clean "msgcat --no-location -"
```
