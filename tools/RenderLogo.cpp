// Rebuild application icons and installer artwork from the bundled SVGs.
// cmake --build build --target ClipStreamLogo
// build/ClipStreamLogo.exe resources
#include <QGuiApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPainterPath>
#include <QXmlStreamWriter>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QVector>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    const QDir dir(app.arguments().value(1, QStringLiteral("resources")));
    // Outline the reference's Sora SemiBold lettering, so the app needs no font installation.
    const int fontId = QFontDatabase::addApplicationFont(dir.filePath(QStringLiteral("fonts/Sora.ttf")));
    if (fontId < 0) return 10;
    QFont font(QFontDatabase::applicationFontFamilies(fontId).first());
    font.setPixelSize(170);
    font.setWeight(QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, -3.06);
    QPainterPath lettering;
    lettering.addText(QPointF(0, 0), font, QStringLiteral("ClipStream"));
    QString pathData;
    for (int i = 0; i < lettering.elementCount(); ++i) {
        const auto e = lettering.elementAt(i);
        if (e.isMoveTo()) pathData += QStringLiteral("M%1 %2").arg(e.x).arg(e.y);
        else if (e.isLineTo()) pathData += QStringLiteral("L%1 %2").arg(e.x).arg(e.y);
        else if (e.type == QPainterPath::CurveToElement) {
            const auto c = lettering.elementAt(++i);
            const auto end = lettering.elementAt(++i);
            pathData += QStringLiteral("C%1 %2 %3 %4 %5 %6").arg(e.x).arg(e.y).arg(c.x).arg(c.y).arg(end.x).arg(end.y);
        }
    }
    const QRectF bounds = lettering.boundingRect();
    for (bool dark : {false, true}) {
        QFile svg(dir.filePath(dark ? QStringLiteral("wordmark-dark.svg") : QStringLiteral("wordmark.svg")));
        if (!svg.open(QIODevice::WriteOnly)) return 11;
        QXmlStreamWriter xml(&svg);
        xml.writeStartElement(QStringLiteral("svg"));
        xml.writeDefaultNamespace(QStringLiteral("http://www.w3.org/2000/svg"));
        xml.writeAttribute(QStringLiteral("viewBox"), QStringLiteral("%1 %2 %3 %4").arg(bounds.x()-1).arg(bounds.y()-1).arg(bounds.width()+2).arg(bounds.height()+2));
        xml.writeAttribute(QStringLiteral("width"), QString::number(bounds.width()+2));
        xml.writeAttribute(QStringLiteral("height"), QString::number(bounds.height()+2));
        xml.writeEmptyElement(QStringLiteral("path"));
        xml.writeAttribute(QStringLiteral("d"), pathData);
        xml.writeAttribute(QStringLiteral("fill"), dark ? QStringLiteral("#FFFFFF") : QStringLiteral("#121316"));
        xml.writeEndElement();
        if (xml.hasError()) return 12;

        // One horizontal lockup keeps the README symbol and lettering aligned.
        QFile mark(dir.filePath(dark ? QStringLiteral("logo-dark.svg") : QStringLiteral("logo.svg")));
        if (!mark.open(QIODevice::ReadOnly)) return 13;
        const QString symbolSvg = QString::fromUtf8(mark.readAll());
        const int start = symbolSvg.indexOf(QLatin1Char('>')) + 1;
        const int end = symbolSvg.lastIndexOf(QStringLiteral("</svg>"));
        if (start <= 0 || end < start) return 14;
        const qreal scale = qMin(568.0/(bounds.width()+2), 84.0/(bounds.height()+2));
        const qreal x = 152 + (568-(bounds.width()+2)*scale)/2 - (bounds.x()-1)*scale;
        const qreal y = 18 + (84-(bounds.height()+2)*scale)/2 - (bounds.y()-1)*scale;
        const QString lockup = QStringLiteral(
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"720\" height=\"120\" viewBox=\"0 0 720 120\">"
            "<g>%1</g><g transform=\"translate(%2 %3) scale(%4)\">"
            "<path d=\"%5\" fill=\"%6\"/></g></svg>")
            .arg(symbolSvg.mid(start, end-start))
            .arg(x).arg(y).arg(scale)
            .arg(pathData, dark ? QStringLiteral("#FFFFFF") : QStringLiteral("#121316"));
        QFile readme(dir.filePath(dark ? QStringLiteral("readme-logo-dark.svg") : QStringLiteral("readme-logo.svg")));
        if (!readme.open(QIODevice::WriteOnly) || readme.write(lockup.toUtf8()) < 0) return 15;
    }
    QSvgRenderer renderer(dir.filePath(QStringLiteral("app-icon.svg")));
    if (!renderer.isValid()) return 1;
    auto render = [&renderer](int size) {
        QImage image(size, size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        return image;
    };
    if (!render(512).save(dir.filePath(QStringLiteral("icon.png")))) return 2;

    // Render at 4x the classic wizard size for crisp artwork on high-DPI displays.
    QSvgRenderer wordmark(dir.filePath(QStringLiteral("wordmark-dark.svg")));
    QSvgRenderer symbol(dir.filePath(QStringLiteral("logo-dark.svg")));
    if (!wordmark.isValid() || !symbol.isValid()) return 7;
    QImage panel(656, 1256, QImage::Format_RGB32);
    panel.fill(QColor(QStringLiteral("#16171B")));
    {
        QPainter painter(&panel);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(4, 4);
        wordmark.render(&painter, QRectF(16, 28, 132, 25));
        symbol.render(&painter, QRectF(34, 108, 96, 90));
        painter.setPen(QPen(QColor(QStringLiteral("#E4472B")), 2));
        painter.drawLine(QPointF(16, 252), QPointF(48, 252));
        QFont font(QStringLiteral("Segoe UI"));
        font.setPixelSize(11);
        painter.setFont(font);
        painter.setPen(QColor(QStringLiteral("#F7F6F2")));
        painter.drawText(QRectF(16, 264, 132, 36), Qt::AlignLeft, QStringLiteral("Your clipboard,\nin one place."));
    }
    if (!panel.save(dir.filePath(QStringLiteral("installer-wizard.bmp")))) return 8;
    QImage small(220, 220, QImage::Format_RGB32);
    small.fill(Qt::white);
    {
        QPainter painter(&small);
        renderer.render(&painter, QRectF(10, 10, 200, 200));
    }
    if (!small.save(dir.filePath(QStringLiteral("installer-header.bmp")))) return 9;

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
