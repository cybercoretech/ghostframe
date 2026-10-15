import QtQuick
import QtQuick.Controls

// Paged photo grid backed by PhotoService; loads more at the bottom.
GridView {
    id: grid
    signal photoOpened(var photo)

    cellWidth: 216; cellHeight: 194
    model: photoService
    clip: true
    cacheBuffer: 1200
    delegate: PhotoCard {
        width: grid.cellWidth - 12; height: grid.cellHeight - 12
        onOpened: (photo) => grid.photoOpened(photo)
    }
    onAtYEndChanged: if (atYEnd) photoService.loadMore()
    ScrollBar.vertical: ScrollBar { active: true }
}
