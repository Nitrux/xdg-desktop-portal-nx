// SPDX-License-Identifier: BSD-3-Clause
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.mauikit.controls as Maui

Maui.ApplicationWindow {
    id: root

    required property var bridge
    property string dialogTitle: qsTr("Choose an Application")
    property string subject: ""
    property bool modalDialog: true
    property var applications: []
    property string lastChoice: ""
    property string selectedId: ""
    property bool completed: false

    title: dialogTitle
    width: 620
    height: 560
    minimumWidth: 440
    minimumHeight: 360
    visible: true
    modality: modalDialog ? Qt.ApplicationModal : Qt.NonModal

    function rebuild() {
        appModel.clear()
        const needle = searchField.text.toLocaleLowerCase()
        for (let i = 0; i < applications.length; ++i) {
            const app = applications[i]
            const haystack = (app.name + " " + app.comment + " " + app.desktopId).toLocaleLowerCase()
            if (needle.length === 0 || haystack.indexOf(needle) !== -1)
                appModel.append(app)
        }
    }

    function acceptSelection() {
        if (selectedId.length === 0)
            return
        completed = true
        bridge.accept(selectedId)
        close()
    }

    ListModel {
        id: appModel
    }

    Maui.Page {
        anchors.fill: parent
        title: root.dialogTitle

        headBar.middleContent: Maui.SearchField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search applications…")
            onTextChanged: root.rebuild()
        }

        footer: Maui.ToolBar {
            position: ToolBar.Footer

            rightContent: [
                Button {
                    text: qsTr("Cancel")
                    onClicked: {
                        root.completed = true
                        root.bridge.reject()
                        root.close()
                    }
                },
                Button {
                    text: qsTr("Open")
                    enabled: root.selectedId.length > 0
                    highlighted: true
                    onClicked: root.acceptSelection()
                }
            ]
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Maui.Style.space.medium

            Label {
                Layout.fillWidth: true
                Layout.margins: Maui.Style.space.medium
                visible: root.subject.length > 0
                text: qsTr("Open %1 with:").arg(root.subject)
                elide: Text.ElideMiddle
            }

            Maui.ListBrowser {
                id: appList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: appModel
                currentIndex: -1

                delegate: Maui.ListBrowserDelegate {
                    width: ListView.view.width
                    isCurrentItem: root.selectedId === model.desktopId
                    template.iconSource: model.icon.length > 0 ? model.icon : "application-x-executable"
                    template.label1.text: model.name
                    template.label2.text: model.comment.length > 0 ? model.comment : model.desktopId
                    onClicked: root.selectedId = model.desktopId
                    onDoubleClicked: {
                        root.selectedId = model.desktopId
                        root.acceptSelection()
                    }
                }
            }
        }
    }

    Connections {
        target: root.bridge
        function onCloseUiRequested() {
            root.completed = true
            root.close()
        }
    }

    onApplicationsChanged: rebuild()
    Component.onCompleted: {
        rebuild()
        for (let i = 0; i < applications.length; ++i) {
            if (applications[i].desktopId === lastChoice) {
                selectedId = lastChoice
                break
            }
        }
    }
    onClosing: (close) => {
        if (!completed) {
            completed = true
            bridge.reject()
        }
    }
}

