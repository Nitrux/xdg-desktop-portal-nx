// SPDX-License-Identifier: BSD-3-Clause
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.mauikit.controls as Maui

Maui.ApplicationWindow
{
    id: root

    visible: true
    width: 760
    height: 720
    minimumWidth: 560
    minimumHeight: 520
    title: qsTr("Nitrux Portal Test")
    color: "transparent"
    background: null

    Maui.WindowBlur
    {
        view: root
        geometry: Qt.rect(0, 0, root.width, root.height)
        windowRadius: Maui.Style.radiusV
        enabled: true
    }

    Rectangle
    {
        anchors.fill: parent
        color: Maui.Theme.backgroundColor
        opacity: 0.76
        radius: Maui.Style.radiusV
        border.color: Qt.rgba(1, 1, 1, 0)
        border.width: 1
    }

    readonly property int buttonWidth: Maui.Style.units.gridUnit * 13

    Maui.ScrollColumn
    {
        background: null
        anchors.fill: parent
        anchors.margins: Maui.Style.contentMargins
        spacing: Maui.Style.space.big

        Maui.SectionHeader
        {
            Layout.fillWidth: true
            text1: qsTr("Portal Test")
            text2: qsTr("Exercise desktop portal requests and inspect their results in the terminal.")
            label2.wrapMode: Text.Wrap
        }

        Rectangle
        {
            Layout.fillWidth: true
            color: Maui.Theme.alternateBackgroundColor
            radius: Maui.Style.radiusV
            border.color: Maui.Theme.backgroundColor
            border.width: 1
            implicitHeight: fileChooserCard.implicitHeight + Maui.Style.contentMargins * 2

            ColumnLayout
            {
                id: fileChooserCard
                anchors.fill: parent
                anchors.margins: Maui.Style.contentMargins
                spacing: Maui.Style.space.small

                Maui.SectionHeader
                {
                    Layout.fillWidth: true
                    text1: qsTr("File chooser")
                    text2: qsTr("Open the portal file dialog, navigate between folders, and return a selection.")
                    label2.wrapMode: Text.Wrap
                }

                Button
                {
                    Layout.alignment: Qt.AlignRight
                    Layout.preferredWidth: root.buttonWidth
                    text: qsTr("File chooser and navigation")
                    onClicked: portalTest.fileChooser()
                }
            }
        }

        Rectangle
        {
            Layout.fillWidth: true
            color: Maui.Theme.alternateBackgroundColor
            radius: Maui.Style.radiusV
            border.color: Maui.Theme.backgroundColor
            border.width: 1
            implicitHeight: uriCard.implicitHeight + Maui.Style.contentMargins * 2

            ColumnLayout
            {
                id: uriCard
                anchors.fill: parent
                anchors.margins: Maui.Style.contentMargins
                spacing: Maui.Style.space.small

                Maui.SectionHeader
                {
                    Layout.fillWidth: true
                    text1: qsTr("OpenURI")
                    text2: qsTr("Test URL opening and launch a desktop application through the host integration.")
                    label2.wrapMode: Text.Wrap
                }

                RowLayout
                {
                    Layout.alignment: Qt.AlignRight
                    spacing: Maui.Style.space.small

                    Button
                    {
                        Layout.preferredWidth: root.buttonWidth
                        text: qsTr("Open URL")
                        onClicked: portalTest.openUri()
                    }

                    Button
                    {
                        Layout.preferredWidth: root.buttonWidth
                        text: qsTr("Launch application")
                        onClicked: portalTest.openWithApplication()
                    }
                }
            }
        }

        Rectangle
        {
            Layout.fillWidth: true
            color: Maui.Theme.alternateBackgroundColor
            radius: Maui.Style.radiusV
            border.color: Maui.Theme.backgroundColor
            border.width: 1
            implicitHeight: oauthCard.implicitHeight + Maui.Style.contentMargins * 2

            ColumnLayout
            {
                id: oauthCard
                anchors.fill: parent
                anchors.margins: Maui.Style.contentMargins
                spacing: Maui.Style.space.small

                Maui.SectionHeader
                {
                    Layout.fillWidth: true
                    text1: qsTr("OAuth redirect")
                    text2: qsTr("Open an authorization page, then simulate its callback through OpenURI with ask enabled.")
                    label2.wrapMode: Text.Wrap
                }

                RowLayout
                {
                    Layout.alignment: Qt.AlignRight
                    spacing: Maui.Style.space.small

                    Button
                    {
                        Layout.preferredWidth: root.buttonWidth
                        text: qsTr("Open authorization page")
                        onClicked: portalTest.oauthAuthorize()
                    }

                    Button
                    {
                        Layout.preferredWidth: root.buttonWidth
                        text: qsTr("Simulate redirect")
                        onClicked: portalTest.oauthCallback()
                    }
                }
            }
        }

        Label
        {
            Layout.fillWidth: true
            text: qsTr("All request replies and cancellation results are printed with qDebug() in the terminal.")
            wrapMode: Text.Wrap
            opacity: 0.7
        }
    }
}
