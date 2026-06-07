# Contributor: Devin J. Pohly <djpohly+arch@gmail.com>
# Maintainer: Alastair Hughes <hobbitalastair@gmail.com>

pkgname=dwl
pkgver=v0.8.dev.r96.gcf954aa
pkgrel=1
pkgdesc="Simple, hackable dynamic tiling Wayland compositor (dwm for Wayland)"
arch=('x86_64')
url="https://codeberg.org/dwl/dwl"
license=('GPL')
depends=('xorg-xwayland' 'mew-git' 'st' 'fcft' 'pixman')
makedepends=('wayland-protocols' 'tllist' 'git' 'meson')
source=("${pkgname}::git+file://${PWD}#branch=patched"
        "wlroots::git+https://gitlab.freedesktop.org/wlroots/wlroots.git#branch=0.19")
sha256sums=('SKIP'
            'SKIP')

pkgver() {
    cd "$pkgname"
    git describe --long --abbrev=7 | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

prepare() {
    cd "$srcdir/$pkgname"
    git submodule init
    git config submodule.wlroots.url "$srcdir/wlroots"
    git -c protocol.file.allow=always submodule update
}

build() {
    cd "$srcdir/$pkgname"
    make
}

package() {
    cd "$srcdir/$pkgname"
    make DESTDIR="$pkgdir" PREFIX=/usr install
}
