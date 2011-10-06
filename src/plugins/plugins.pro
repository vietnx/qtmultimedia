######################################################################
#
# QtMultimedia
#
######################################################################

TEMPLATE = subdirs

SUBDIRS += m3u

win32 {
    SUBDIRS += audiocapture
}

win32 {
    contains(config_test_directshow, yes): SUBDIRS += directshow
    contains(config_test_wmf, yes) : SUBDIRS += wmf
}

simulator: SUBDIRS += simulator

unix:!mac {
    TMP_GST_LIBS = \
        gstreamer-0.10 >= 0.10.19 \
        gstreamer-base-0.10 >= 0.10.19 \
        gstreamer-interfaces-0.10 >= 0.10.19 \
        gstreamer-audio-0.10 >= 0.10.19 \
        gstreamer-video-0.10 >= 0.10.19

    system(pkg-config --exists \'$${TMP_GST_LIBS}\' --print-errors): {
        SUBDIRS += gstreamer
    } else {
        SUBDIRS += audiocapture
    }

    # v4l is turned off because it is not supported in Qt 5
    # !maemo*:SUBDIRS += v4l

    contains(config_test_pulseaudio, yes) {
        SUBDIRS += pulseaudio
    }
}

mac:!simulator {
    SUBDIRS += audiocapture
    !qpa: SUBDIRS += qt7
}

# fake radio to test the radio APIs
SUBDIRS += fakeradio

