## Preparing a "XX.po" file for commit

To save a location-less "po/XX.po" automatically in the repository,
define the driver for the "gettext-no-location" clean filter like this:

```shell
git config filter.gettext-no-location.clean "msgcat --no-location -"
```
