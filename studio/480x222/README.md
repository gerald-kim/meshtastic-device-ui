# T-LoRa Pager 480x222 EEZ source

Generate the EEZ-owned C files with the official EEZ Studio 0.29.0 release:

```sh
xvfb-run -a eez-studio --build-project "$PWD/studio/480x222/TFT480x222.eez-project"
```

The generated manifest is `generated/ui_480x222/.eez-project-build`. Files not listed in that manifest are shared manual assets copied from `generated/ui_320x240`; EEZ Studio preserves them during regeneration.
