# qtmultimedia
Qt Multimedia with some modified audio plugins for better audio output

## Support WASAPI output exclusive mode
With modified WASAPI audio plugin, we can build this plugin to support WASAPI shared or exclusive mode by change the
following defined in "src/plugins/wasapi/qwasapiutils.h"

    #define WASAPI_MODE AUDCLNT_SHAREMODE_SHARED
