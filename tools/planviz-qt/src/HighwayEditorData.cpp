#include "HighwayEditorData.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

quint64 HighwayEditorData::key(int row, int col)
{
    return (static_cast<quint64>(static_cast<quint32>(row)) << 32) |
           static_cast<quint32>(col);
}

QPoint HighwayEditorData::cellFromKey(quint64 packed)
{
    return QPoint(static_cast<int>(packed & 0xffffffffu),
                  static_cast<int>((packed >> 32) & 0xffffffffu));
}

int HighwayEditorData::normalizeDir(int dir)
{
    int result = dir % 4;
    if (result < 0) {
        result += 4;
    }
    return result;
}

bool HighwayEditorData::loadMap(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = "Cannot open map file: " + path;
        return false;
    }

    const QString text = QString::fromUtf8(file.readAll()).replace('\r', "");
    const QStringList lines = text.split('\n');
    int mapStart = -1;
    int height = -1;
    int width = -1;
    static const QRegularExpression mapLineRe(QStringLiteral("^[.@TGSW_]+$"),
                                              QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        const QString lower = line.toLower();
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                             Qt::SkipEmptyParts);
        if (parts.size() >= 2 && parts[0].compare("height", Qt::CaseInsensitive) == 0) {
            height = parts[1].toInt();
        } else if (parts.size() >= 2 && parts[0].compare("width", Qt::CaseInsensitive) == 0) {
            width = parts[1].toInt();
        } else if (lower == "map") {
            mapStart = i + 1;
            break;
        } else if (mapLineRe.match(line).hasMatch()) {
            mapStart = i;
            break;
        }
    }

    if (mapStart < 0 || mapStart >= lines.size()) {
        *error = "Cannot find map body.";
        return false;
    }
    if (height <= 0) {
        height = 0;
        for (int i = mapStart; i < lines.size(); ++i) {
            if (mapLineRe.match(lines[i].trimmed()).hasMatch()) {
                ++height;
            }
        }
    }
    if (width <= 0) {
        width = lines[mapStart].trimmed().size();
    }
    if (height <= 0 || width <= 0) {
        *error = "Invalid map dimensions.";
        return false;
    }

    QVector<unsigned char> nextMap;
    nextMap.reserve(height * width);
    int parsedRows = 0;
    for (int i = mapStart; i < lines.size() && parsedRows < height; ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.size() < width) {
            *error = QString("Map row %1 is too short.").arg(parsedRows);
            return false;
        }
        for (int col = 0; col < width; ++col) {
            const QChar ch = line[col];
            nextMap.push_back((ch == '.' || ch == 'S' || ch == 'E') ? 0 : 1);
        }
        ++parsedRows;
    }

    if (parsedRows != height) {
        *error = QString("Expected %1 map rows, found %2.").arg(height).arg(parsedRows);
        return false;
    }

    m_rows = height;
    m_cols = width;
    m_map = std::move(nextMap);
    return true;
}

bool HighwayEditorData::loadJson(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = "Cannot open JSON file: " + path;
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = "Invalid JSON: " + parseError.errorString();
        return false;
    }

    clearAnnotations();
    const QJsonObject root = doc.object();
    addCellsFromJson(root.value("primary").isUndefined() ? root.value("yellow") : root.value("primary"),
                     &m_primary, &m_secondary);
    addCellsFromJson(root.value("secondary").isUndefined() ? root.value("blue") : root.value("secondary"),
                     &m_secondary, &m_primary);
    const QJsonObject parking = root.value("parking").toObject();
    addParkingFromJson(parking.value("upper"), ParkingTarget::Upper);
    addParkingFromJson(parking.value("lower"), ParkingTarget::Lower);
    addParkingFromJson(root.value("parkingUpper"), ParkingTarget::Upper);
    addParkingFromJson(root.value("upperParking"), ParkingTarget::Upper);
    addParkingFromJson(root.value("upper_parking"), ParkingTarget::Upper);
    addParkingFromJson(root.value("parkingLower"), ParkingTarget::Lower);
    addParkingFromJson(root.value("lowerParking"), ParkingTarget::Lower);
    addParkingFromJson(root.value("lower_parking"), ParkingTarget::Lower);
    addManualPenaltyFromJson(root.value("manualPenalty"));
    addManualPenaltyFromJson(root.value("manual_penalty"));
    addManualPenaltyFromJson(root.value("red"));
    addManualPenaltyFromJson(root.value("red_cells"));
    addHighwayDirectionsFromJson(root.value("highwayDirections"));
    return true;
}

bool HighwayEditorData::saveJson(const QString& path, QString* error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = "Cannot write JSON file: " + path;
        return false;
    }
    file.write(exportJsonText().toUtf8());
    return true;
}

QString HighwayEditorData::exportJsonText() const
{
    QJsonObject parking;
    parking.insert("upper", cellsToJson(m_upperParking));
    parking.insert("lower", cellsToJson(m_lowerParking));

    QJsonArray manual;
    for (const HighwayArrowMarker& item : sortedManualPenalty()) {
        QJsonArray entry;
        entry.push_back(item.row);
        entry.push_back(item.col);
        entry.push_back(item.dir);
        manual.push_back(entry);
    }

    QJsonObject root;
    root.insert("primary", cellsToJson(m_primary));
    root.insert("secondary", cellsToJson(m_secondary));
    root.insert("parking", parking);
    root.insert("manualPenalty", manual);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString HighwayEditorData::exportCppText() const
{
    auto cppArray = [](const QString& name, const QVector<QPoint>& cells) {
        QString out = QString("const std::array<std::pair<int, int>, %1> %2 = {\n")
                          .arg(cells.size())
                          .arg(name);
        for (const QPoint& p : cells) {
            out += QString("        std::pair<int, int>{%1, %2},\n").arg(p.y()).arg(p.x());
        }
        out += "};";
        return out;
    };

    QString out;
    out += cppArray("orz_c_tube_primary_cells", sortedCells(m_primary));
    out += "\n\n";
    out += cppArray("orz_c_tube_secondary_cells", sortedCells(m_secondary));
    out += "\n\n";
    out += cppArray("orz_upper_parking_cells", sortedCells(m_upperParking));
    out += "\n\n";
    out += cppArray("orz_lower_parking_cells", sortedCells(m_lowerParking));
    out += "\n\n";
    const QVector<HighwayArrowMarker> manual = sortedManualPenalty();
    out += "struct OrzManualPenaltyCell { int row; int col; int dir; };\n";
    out += "// dir order: 0=right, 1=down, 2=left, 3=up\n";
    out += QString("const std::array<OrzManualPenaltyCell, %1> orz_c_tube_manual_penalty_cells = {\n")
               .arg(manual.size());
    for (const HighwayArrowMarker& item : manual) {
        out += QString("        OrzManualPenaltyCell{%1, %2, %3},\n")
                   .arg(item.row)
                   .arg(item.col)
                   .arg(item.dir);
    }
    out += "};";
    return out;
}

void HighwayEditorData::clearAnnotations()
{
    m_primary.clear();
    m_secondary.clear();
    m_upperParking.clear();
    m_lowerParking.clear();
    m_manualPenalty.clear();
    m_highwayDirections.clear();
}

bool HighwayEditorData::blocked(int row, int col) const
{
    if (!inBounds(row, col)) {
        return true;
    }
    return m_map[row * m_cols + col] != 0;
}

bool HighwayEditorData::inBounds(int row, int col) const
{
    return row >= 0 && col >= 0 && row < m_rows && col < m_cols;
}

void HighwayEditorData::paint(int row, int col, Layer layer, int manualMask)
{
    if (!inBounds(row, col)) {
        return;
    }
    const quint64 k = key(row, col);
    clearCell(k);
    switch (layer) {
    case Layer::Primary:
        m_primary.insert(k);
        break;
    case Layer::Secondary:
        m_secondary.insert(k);
        break;
    case Layer::UpperParking:
        m_upperParking.insert(k);
        break;
    case Layer::LowerParking:
        m_lowerParking.insert(k);
        break;
    case Layer::ManualPenalty:
        if (manualMask != 0) {
            m_manualPenalty.insert(k, manualMask & 0xf);
        }
        break;
    case Layer::Erase:
        break;
    }
}

HighwayEditorData::CellState HighwayEditorData::cellStateAt(int row, int col) const
{
    const quint64 k = key(row, col);
    CellState state;
    state.primary = m_primary.contains(k);
    state.secondary = m_secondary.contains(k);
    state.upperParking = m_upperParking.contains(k);
    state.lowerParking = m_lowerParking.contains(k);
    state.manualMask = m_manualPenalty.value(k, 0);
    return state;
}

void HighwayEditorData::restoreCellState(int row, int col, const CellState& state)
{
    if (!inBounds(row, col)) {
        return;
    }
    const quint64 k = key(row, col);
    clearCell(k);
    if (state.primary) {
        m_primary.insert(k);
    }
    if (state.secondary) {
        m_secondary.insert(k);
    }
    if (state.upperParking) {
        m_upperParking.insert(k);
    }
    if (state.lowerParking) {
        m_lowerParking.insert(k);
    }
    if (state.manualMask != 0) {
        m_manualPenalty.insert(k, state.manualMask & 0xf);
    }
}

int HighwayEditorData::manualMaskAt(int row, int col) const
{
    return m_manualPenalty.value(key(row, col), 0);
}

int HighwayEditorData::manualPenaltyMoveCount() const
{
    int count = 0;
    for (auto it = m_manualPenalty.cbegin(); it != m_manualPenalty.cend(); ++it) {
        int mask = it.value();
        for (int dir = 0; dir < 4; ++dir) {
            if (mask & dirMask(dir)) {
                ++count;
            }
        }
    }
    return count;
}

void HighwayEditorData::clearCell(quint64 k)
{
    m_primary.remove(k);
    m_secondary.remove(k);
    m_upperParking.remove(k);
    m_lowerParking.remove(k);
    m_manualPenalty.remove(k);
}

void HighwayEditorData::addCellsFromJson(const QJsonValue& value, CellSet* target, CellSet* other)
{
    if (!value.isArray()) {
        return;
    }
    for (const QJsonValue& item : value.toArray()) {
        addCellValueToSet(item, target, other);
    }
}

void HighwayEditorData::addParkingFromJson(const QJsonValue& value, ParkingTarget target)
{
    CellSet* set = target == ParkingTarget::Upper ? &m_upperParking : &m_lowerParking;
    addCellsFromJson(value, set, nullptr);
}

void HighwayEditorData::addManualPenaltyFromJson(const QJsonValue& value)
{
    if (!value.isArray()) {
        return;
    }
    for (const QJsonValue& item : value.toArray()) {
        int row = 0;
        int col = 0;
        QVector<int> dirs;
        if (item.isArray()) {
            const QJsonArray arr = item.toArray();
            if (arr.size() >= 2) {
                row = arr[0].toInt();
                col = arr[1].toInt();
                if (arr.size() >= 3) {
                    dirs.push_back(arr[2].toInt());
                } else {
                    dirs.push_back(0);
                }
            }
        } else if (item.isObject()) {
            const QJsonObject obj = item.toObject();
            row = obj.value("row").toInt();
            col = obj.value("col").toInt();
            if (obj.value("dirs").isArray()) {
                for (const QJsonValue& dirValue : obj.value("dirs").toArray()) {
                    dirs.push_back(dirValue.toInt());
                }
            } else {
                dirs.push_back(obj.value("dir").toInt(0));
            }
        } else if (item.isString()) {
            const QStringList parts = item.toString().split(',');
            if (parts.size() >= 2) {
                row = parts[0].trimmed().toInt();
                col = parts[1].trimmed().toInt();
                dirs.push_back(parts.size() >= 3 ? parts[2].trimmed().toInt() : 0);
            }
        }
        for (int dir : dirs) {
            addManualPenaltyDir(row, col, dir);
        }
    }
}

void HighwayEditorData::addHighwayDirectionsFromJson(const QJsonValue& value)
{
    if (!value.isArray()) {
        return;
    }
    for (const QJsonValue& item : value.toArray()) {
        if (!item.isObject()) {
            continue;
        }
        const QJsonObject obj = item.toObject();
        HighwayArrowMarker marker;
        marker.row = obj.value("row").toInt();
        marker.col = obj.value("col").toInt();
        marker.dir = normalizeDir(obj.value("dir").toInt());
        marker.blockedDir = obj.value("blockedDir").toInt(-1);
        marker.weight = obj.value("weight").toInt(0);
        if (!inBounds(marker.row, marker.col) && hasMap()) {
            continue;
        }
        m_highwayDirections.push_back(marker);
        addManualPenaltyDir(marker.row, marker.col, marker.dir + 2);
    }
}

void HighwayEditorData::addCellValueToSet(const QJsonValue& value, CellSet* target, CellSet* other)
{
    int row = 0;
    int col = 0;
    bool ok = false;
    if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        if (arr.size() >= 2) {
            row = arr[0].toInt();
            col = arr[1].toInt();
            ok = true;
        }
    } else if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        row = obj.value("row").toInt();
        col = obj.value("col").toInt();
        ok = obj.contains("row") && obj.contains("col");
    } else if (value.isString()) {
        const QStringList parts = value.toString().split(',');
        if (parts.size() >= 2) {
            row = parts[0].trimmed().toInt();
            col = parts[1].trimmed().toInt();
            ok = true;
        }
    }
    if (!ok) {
        return;
    }
    const quint64 k = key(row, col);
    target->insert(k);
    if (other) {
        other->remove(k);
    }
    m_manualPenalty.remove(k);
}

void HighwayEditorData::addManualPenaltyDir(int row, int col, int dir)
{
    const quint64 k = key(row, col);
    m_manualPenalty.insert(k, m_manualPenalty.value(k, 0) | dirMask(normalizeDir(dir)));
    m_primary.remove(k);
    m_secondary.remove(k);
    m_upperParking.remove(k);
    m_lowerParking.remove(k);
}

QVector<QPoint> HighwayEditorData::sortedCells(const CellSet& set) const
{
    QVector<QPoint> cells;
    cells.reserve(set.size());
    for (quint64 k : set) {
        cells.push_back(cellFromKey(k));
    }
    std::sort(cells.begin(), cells.end(), [](const QPoint& a, const QPoint& b) {
        if (a.y() != b.y()) {
            return a.y() < b.y();
        }
        return a.x() < b.x();
    });
    return cells;
}

QVector<HighwayArrowMarker> HighwayEditorData::sortedManualPenalty() const
{
    QVector<HighwayArrowMarker> items;
    for (auto it = m_manualPenalty.cbegin(); it != m_manualPenalty.cend(); ++it) {
        const QPoint p = cellFromKey(it.key());
        for (int dir = 0; dir < 4; ++dir) {
            if (it.value() & dirMask(dir)) {
                HighwayArrowMarker item;
                item.row = p.y();
                item.col = p.x();
                item.dir = dir;
                items.push_back(item);
            }
        }
    }
    std::sort(items.begin(), items.end(), [](const HighwayArrowMarker& a, const HighwayArrowMarker& b) {
        if (a.row != b.row) {
            return a.row < b.row;
        }
        if (a.col != b.col) {
            return a.col < b.col;
        }
        return a.dir < b.dir;
    });
    return items;
}

QJsonArray HighwayEditorData::cellsToJson(const CellSet& set) const
{
    QJsonArray arr;
    for (const QPoint& p : sortedCells(set)) {
        QJsonArray entry;
        entry.push_back(p.y());
        entry.push_back(p.x());
        arr.push_back(entry);
    }
    return arr;
}
