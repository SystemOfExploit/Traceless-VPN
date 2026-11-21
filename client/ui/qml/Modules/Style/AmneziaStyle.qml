pragma Singleton

import QtQuick

QtObject {
    property QtObject color: QtObject {
        readonly property color transparent: 'transparent'

        readonly property color textPrimary: '#FAFAFA'
        readonly property color textSecondary: '#E4E4E4'
        readonly property color textTertiary: '#A1A1A1'
        readonly property color textQuaternary: '#868686'
        readonly property color textInverted: '#090909'
        readonly property color textAccent: '#E8E8E8'
        readonly property color textInfoInverse: '#333333'
        readonly property color textStaticWhite: '#FFFFFF'

        readonly property color bgBase: '#101010'
        readonly property color bgSurface: '#181818'
        readonly property color bgSurfaceAlt: '#212121'
        readonly property color bgSheet: '#303030'
        readonly property color bgPanel: '#3D3D3D'
        readonly property color bgPanelAlt: '#505050'
        readonly property color bgOverlay: '#80090909'
        readonly property color bgStateLayer: '#14FAFAFA'
        readonly property color bgStateLayerPressed: '#1AFAFAFA'
        readonly property color bgStateLayerAccent: '#20FFFFFF'
        readonly property color bgSurfaceInverse: '#E4E4E4'
        readonly property color bgSurfaceInverseAlt: '#D4D4D4'
        readonly property color bgControlTrack: '#1F1F1F'
        readonly property color bgField: '#1F1F1F'
        readonly property color bgSelected: '#1F1F1F'
        readonly property color bgAccentSubtle: '#32E5E5E5'
        readonly property color bgStaticWhite: '#FFFFFF'

        readonly property color bgAction: '#E4E4E4'
        readonly property color bgActionHover: '#D4D4D4'
        readonly property color bgActionPress: '#A1A1A1'

        readonly property color borderFaint: '#313131'
        readonly property color borderSoft: '#3F3F3F'
        readonly property color borderMedium: '#525252'
        readonly property color borderBold: '#A1A1A1'
        readonly property color borderEmphasis: '#E4E4E4'
        readonly property color borderAccent: '#E8E8E8'
        readonly property color borderField: '#525252'
        readonly property color borderFocus: '#E4E4E4'

        readonly property color accentLight: '#F1F1F1'
        readonly property color accentDefault: '#D0D0D0'
        readonly property color accentDark: '#A0A0A0'
        readonly property color accentDeep: '#555555'

        readonly property color statusDestructive: '#F5F5F5'
        readonly property color statusWarning: '#D5D5D5'
        readonly property color statusSuccess: '#EEEEEE'

        readonly property color promoRecommendation: '#DEDEDE'
        readonly property color promoUpdate: '#C7C7C7'
        readonly property color promoOverlay: '#16FFFFFF'
        readonly property color promoPink: '#C8C8C8'
        readonly property color promoPinkDeep: '#666666'

        readonly property color iconDefault: '#E4E4E4'
        readonly property color iconSecondary: '#A1A1A1'
        readonly property color iconTertiary: '#3F3F3F'
        readonly property color iconInverted: '#090909'

        readonly property color shadowLayer1: '#26000000'
        readonly property color shadowLayer2: '#4D000000'
        readonly property color shadowGlow: '#DADADA'
    }
}
