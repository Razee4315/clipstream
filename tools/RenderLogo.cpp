// Rebuild the PNG and multi-resolution Windows icon from resources/logo.svg.
// cmake --build build --target ClipStreamLogo
// build/ClipStreamLogo.exe resources
#include <QGuiApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QVector>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const QDir dir(app.arguments().value(1, QStringLiteral("resources")));
    QSvgRenderer renderer(dir.filePath(QStringLiteral("logo.svg")));
    if (!renderer.isValid()) return 1;
    auto render = [&renderer](int size) {
        QImage image(size, size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        return image;
    };
    if (!render(512).save(dir.filePath(QStringLiteral("icon.png")))) return 2;

    const QVector<int> sizes{16, 20, 24, 32, 48, 64, 128, 256};
    QVector<QByteArray> images;
    for (int size : sizes) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!render(size).save(&buffer, "PNG")) return 3;
        images.append(bytes);
    }
    QFile file(dir.filePath(QStringLiteral("icon.ico")));
    if (!file.open(QIODevice::WriteOnly)) return 4;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0) << quint16(1) << quint16(sizes.size());
    quint32 offset = 6 + 16 * sizes.size();
    for (int i = 0; i < sizes.size(); ++i) {
        const quint8 dimension = sizes[i] == 256 ? 0 : sizes[i];
        stream << dimension << dimension << quint8(0) << quint8(0)
               << quint16(1) << quint16(32) << quint32(images[i].size()) << offset;
        offset += images[i].size();
    }
    for (const QByteArray& bytes : images)
        if (stream.writeRawData(bytes.constData(), bytes.size()) != bytes.size()) return 5;
    return stream.status() == QDataStream::Ok ? 0 : 6;
}
