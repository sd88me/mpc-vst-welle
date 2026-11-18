Welle 0.3.1: your favourites now survive an upgrade.

**Fixed**
- **Favourites are kept when you update.** The installer now lists `Content/favourites.txt` as your data, next to your Wave OS files and sound sets, so installing a new version (with the OpenMPC Plugin Manager or `install.sh`) keeps your favourites. Before, an upgrade deleted them.

**README**
- Install steps per format (the `VST2 Plugins` folder or `install.sh`, by hand or with the Plugin Manager), and a new section on backing up and restoring your favourites, ROMs and sound sets.

Nothing else changed: same sound, screens and parameters, so saved projects load as before.

**Install**
- With the OpenMPC Plugin Manager: install or update Welle; your Wave OS files, sound sets and favourites stay in `Content/`.
- By hand: unzip `Welle-0.3.1-VST2-Plugins.zip`, copy the folder into `VST2 Plugins`, put `w2sys.bin` and `wdv.sys` in `Content/roms` and your sound sets in `Content/banks`, restart. When copying over an older Welle folder, keep its `Content/` (your files and `favourites.txt`).

No firmware or sounds are included: you need your own copy of Wave OS 1.700.
