# Contributor: Devin J. Pohly <djpohly+arch@gmail.com>
# Maintainer: Alastair Hughes <hobbitalastair@gmail.com>

pkgname=dwl
pkgver=v0.7.r22.g2c337c1
pkgrel=1
pkgdesc="Simple, hackable dynamic tiling Wayland compositor (dwm for Wayland)"
arch=('x86_64')
url="https://codeberg.org/dwl/dwl"
license=('GPL')
depends=('wlroots0.19' 'xorg-xwayland' 'mew-git' 'st' 'fcft' 'pixman')
makedepends=('wayland-protocols' 'tllist' 'git')
source=("${pkgname}::git+file://${PWD}#branch=patched")
sha256sums=('SKIP')

pkgver() {
    cd "$pkgname"
    git describe --long --abbrev=7 | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
    cd "$srcdir/$pkgname"
    make
}

package() {
    cd "$srcdir/$pkgname"
    make PREFIX="$pkgdir/usr/" install
}
