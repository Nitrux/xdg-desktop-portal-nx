import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.mauikit.controls as Maui
import org.mauikit.filebrowsing as FB

/**
 * @inherit org::mauikit::controls::ListBrowser
 * @brief A browsing list of the system locations, such as common standard places, bookmarks and others as removable devices and networks.
 * 
 * This control inherits from MauiKit ListBrowser, to checkout its inherited properties refer to docs.
 * 
 * Most of the properties to control the behaviour is handled via the PlacesList model, which is exposed via the `list` property.
 * @see list
 * 
 * @image html placeslistbrowser.png
 *
 * @code
 * Maui.SideBarView
 * {
 *    anchors.fill: parent
 * 
 *    sideBar.content: Pane
 *    {
 *        Maui.Theme.colorSet: Maui.Theme.Window
 *        anchors.fill: parent
 *        FB.PlacesListBrowser
 *        {
 *            anchors.fill: parent
 *        }
 *    }
 * 
 *    Maui.Page
 *    {
 *        Maui.Controls.showCSD: true
 *        anchors.fill: parent
 *    }
 * }
 *  @endcode
 * 
 * <a href="https://invent.kde.org/maui/mauikit-filebrowser/examples/PlacesListBrowser.qml">You can find a more complete example at this link.</a>
 */
Maui.ListBrowser
{
    id: control

    // The parent FileDialog sizes this item through anchors.
    // Keep implicit height explicit to avoid Maui Pane height recursion.
    implicitHeight: 0

    /**
     * @brief The model list of the places.
     * @property PlacesList PlacesListBrowser::list
     */
    readonly property alias list : placesList
    
    /**
     * @brief The contextual menu for the entries.
     * @note The menu has an extra property `index`, which refers to the index position of the entry where the menu was invoked at.
     * 
     * To add more entries, use the `itemMenu.data` property, or append/push methods.
     * @property Menu PlacesListBrowser::itemMenu
     */
    readonly property alias itemMenu : _menu
    
    /**
     * @brief The preferred size of the icon for the places delegates.
     * By default this is set to `Style.iconSizes.small`
     * @see Style::iconSizes
     */
    property int iconSize : Maui.Style.iconSizes.small
    
    /**
     * @brief The path of the current place selected.
     */
    property string currentPath

    /**
     * @brief A list of place paths to hide from the browser UI.
     */
    property var hiddenPaths : []
    
    /**
     * @brief Emitted when a entry has been clicked.
     * @param path the URL path of the entry 
     */
    signal placeClicked (string path)

    function isRootPath(path)
    {
        const value = String(path)
        return value === "/" || value === "file:///" || value === "file:/"
    }

    function isDeviceSection(type)
    {
        const value = String(type)
        return isStorageSection(value) || value === i18n("Removable")
    }

    function isStorageSection(type)
    {
        const value = String(type)
        if (value === i18n("Drives") || value === i18n("Storage"))
            return true

        if (!placesList)
            return false

        for (let i = 0; i < placesList.count; ++i)
        {
            const item = placesList.get(i)
            if (String(item.type) === value && isRootPath(item.path))
                return true
        }

        return false
    }

    function placeLabel(path, label, type)
    {
        return isStorageSection(type) && isRootPath(path) ? i18n("Root") : String(label)
    }

    function placeIcon(path, iconName, label, type, isDeviceEntry)
    {
        const value = String(path)
        const text = String(label)
        const sectionType = String(type)

        if (isDeviceEntry && isDeviceSection(sectionType))
            return isExternalDeviceSection(sectionType) ? "drive-removable-media" : "drive-harddisk"
        if (value === "/" || value === "file:///")
            return "folder-red"
        if ((value.startsWith("/") || value.startsWith("file:///")) && text.startsWith("/"))
            return "folder"
        return iconName
    }

    function sidebarIcon(path, iconName, label, type, isDeviceEntry)
    {
        const resolved = placeIcon(path, iconName, label, type, isDeviceEntry)
        return resolved === "folder-red" ? resolved : resolved + "-symbolic"
    }

    function usesSymbolicIcon(path, iconName, label, type, isDeviceEntry)
    {
        return placeIcon(path, iconName, label, type, isDeviceEntry) !== "folder-red"
    }

    function isExternalDeviceSection(type)
    {
        return String(type) === i18n("Removable")
    }

    function shouldShowPlace(index, type, path)
    {
        if (!isStorageSection(type))
            return true

        const value = String(path)
        if (isRootPath(value))
            return true

        return placesList.isDevice(index)
    }

    function sectionLabel(type)
    {
        return isStorageSection(type) ? i18n("Storage") : String(type)
    }

    function isPathHidden(path)
    {
        const candidate = path ? path.toString() : ""

        for (var i = 0; i < hiddenPaths.length; ++i)
        {
            const hiddenPath = hiddenPaths[i]
            if ((hiddenPath ? hiddenPath.toString() : "") === candidate)
            {
                return true
            }
        }

        return false
    }
    
    Maui.Theme.colorSet: Maui.Theme.View
    Maui.Theme.inherit: false    
    
    focus: true
    model: Maui.BaseModel
    {
        list: FB.PlacesList
        {
            id: placesList
            groups: [
                FB.FMList.BOOKMARKS_PATH,
                    FB.FMList.REMOTE_PATH,
                    FB.FMList.REMOVABLE_PATH,
                    FB.FMList.DRIVES_PATH]
        }
    }
    
    currentIndex: placesList.indexOfPath(control.currentPath)    
    
    section.property: "type"
    section.criteria: ViewSection.FullString
    section.delegate: Maui.LabelDelegate
    {
        id: delegate
        text: control.sectionLabel(section)
        width: parent.width
        height: Maui.Style.toolBarHeightAlt
    }
    
    Maui.ContextualMenu
    {
        id: _menu
        property int index
        
        MenuItem
        {
            text: i18nd("mauikitfilebrowsing", "Edit")
        }
        
        MenuItem
        {
            text: i18nd("mauikitfilebrowsing", "Hide")
        }
        
        MenuItem
        {
            text: i18nd("mauikitfilebrowsing", "Remove")
            Maui.Controls.status: Maui.Controls.Negative
            onTriggered: list.removePlace(_menu.index)
        }
    }
    
    flickable.header: Loader
    {
        id: _quickSectionLoader
        asynchronous: true
        width: Math.min(parent.width, 180)
        sourceComponent: GridLayout
        {
            id: _quickSection

            rows: 3
            columns: 3
            columnSpacing: Maui.Style.space.small
            rowSpacing: Maui.Style.space.small

            Repeater
            {
                model: Maui.BaseModel
                {
                    list: FB.PlacesList
                    {
                        id: _quickPlacesList
                        groups: [FB.FMList.QUICK_PATH, FB.FMList.PLACES_PATH]
                    }
                }

                delegate: Maui.GridBrowserDelegate
                {
                    Maui.Theme.colorSet: Maui.Theme.Button
                    Maui.Theme.inherit: false

                    readonly property bool hiddenPlace: control.isPathHidden(model.path)
                    Layout.preferredHeight: hiddenPlace ? 0 : Math.min(50, width)
                    Layout.preferredWidth: hiddenPlace ? 0 : 50
                    Layout.fillWidth: !hiddenPlace
                    Layout.fillHeight: !hiddenPlace
                    flat: false
                    visible: !hiddenPlace
                    isCurrentItem: control.currentPath === model.path
                    iconSource: control.sidebarIcon(model.path, model.icon, model.label, model.type, false)
                    iconSizeHint: Maui.Style.iconSize
                    template.isMask: true
                    label1.text: model.label
                    labelsVisible: false
                    tooltipText: model.label
                    onClicked: placeClicked(model.path)
                }
            }
        }
    }

    delegate: Maui.ListDelegate
    {
        readonly property bool hiddenPlace: control.isPathHidden(model.path) || !control.shouldShowPlace(index, model.type, model.path)
        width: ListView.view.width
        height: hiddenPlace ? 0 : implicitHeight
        iconSize: control.iconSize
        labelVisible: true
        iconVisible: true
        label: control.placeLabel(model.path, model.label, model.type)
        iconName: control.sidebarIcon(model.path, model.icon, model.label, model.type, placesList.isDevice(index))
        visible: !hiddenPlace
        enabled: !hiddenPlace
        
        template.content: ToolButton
        {
            visible: placesList.isDevice(index) && control.isExternalDeviceSection(model.type)
            flat: true
            icon.name: placesList.setupNeeded(index) ? "media-mount" : "media-eject"
            icon.width: Maui.Style.iconSizes.small
            icon.height: Maui.Style.iconSizes.small
            onClicked:
            {
                if (placesList.setupNeeded(index))
                    placesList.requestSetup(index)
                else
                    placesList.requestTeardown(index)
            }
        }

        onClicked:
        {
            if (placesList.isDevice(index) && placesList.setupNeeded(index))
            {
                placesList.requestSetup(index)
                return
            }
            placeClicked(model.path)
        }
        
        onRightClicked:
        {
            _menu.index = index
            _menu.popup()
        }
        
        onPressAndHold:
        {
            _menu.index = index
            _menu.popup()
        }
    }
}
