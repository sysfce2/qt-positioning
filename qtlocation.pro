requires(!html5)
load(configure)
qtCompileTest(gypsy)
qtCompileTest(winrt)

load(qt_parts)

DISTFILES += sync.profile configure.json
