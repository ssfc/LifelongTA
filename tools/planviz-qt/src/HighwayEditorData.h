#pragma once

#include <QHash>
#include <QJsonArray>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QVector>

struct HighwayArrowMarker {
    int row = 0;
    int col = 0;
    int dir = 0;
    int blockedDir = -1;
    int weight = 0;
};

class HighwayEditorData {
public:
    struct CellState {
        bool primary = false;
        bool secondary = false;
        bool upperParking = false;
        bool lowerParking = false;
        int manualMask = 0;
    };

    enum class Layer {
        Primary,
        Secondary,
        UpperParking,
        LowerParking,
        ManualPenalty,
        Erase,
    };

    bool loadMap(const QString& path, QString* error);
    bool loadJson(const QString& path, QString* error);
    bool saveJson(const QString& path, QString* error) const;

    QString exportJsonText() const;
    QString exportCppText() const;
    void clearAnnotations();

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    bool hasMap() const { return m_rows > 0 && m_cols > 0 && m_map.size() == m_rows * m_cols; }
    bool blocked(int row, int col) const;

    bool inBounds(int row, int col) const;
    void paint(int row, int col, Layer layer, int manualMask);
    CellState cellStateAt(int row, int col) const;
    void restoreCellState(int row, int col, const CellState& state);
    int manualMaskAt(int row, int col) const;
    bool hasPrimary(int row, int col) const { return m_primary.contains(key(row, col)); }
    bool hasSecondary(int row, int col) const { return m_secondary.contains(key(row, col)); }
    bool hasUpperParking(int row, int col) const { return m_upperParking.contains(key(row, col)); }
    bool hasLowerParking(int row, int col) const { return m_lowerParking.contains(key(row, col)); }

    int primaryCount() const { return m_primary.size(); }
    int secondaryCount() const { return m_secondary.size(); }
    int upperParkingCount() const { return m_upperParking.size(); }
    int lowerParkingCount() const { return m_lowerParking.size(); }
    int manualPenaltyCellCount() const { return m_manualPenalty.size(); }
    int manualPenaltyMoveCount() const;
    int highwayArrowCount() const { return m_highwayDirections.size(); }

    const QVector<HighwayArrowMarker>& highwayDirections() const { return m_highwayDirections; }

    static quint64 key(int row, int col);
    static QPoint cellFromKey(quint64 key);
    static int dirMask(int dir) { return 1 << (dir & 3); }
    static int normalizeDir(int dir);

private:
    enum class ParkingTarget {
        Upper,
        Lower,
    };

    using CellSet = QSet<quint64>;

    void clearCell(quint64 key);
    void addCellsFromJson(const QJsonValue& value, CellSet* target, CellSet* other = nullptr);
    void addParkingFromJson(const QJsonValue& value, ParkingTarget target);
    void addManualPenaltyFromJson(const QJsonValue& value);
    void addHighwayDirectionsFromJson(const QJsonValue& value);
    void addCellValueToSet(const QJsonValue& value, CellSet* target, CellSet* other);
    void addManualPenaltyDir(int row, int col, int dir);
    QVector<QPoint> sortedCells(const CellSet& set) const;
    QVector<HighwayArrowMarker> sortedManualPenalty() const;
    QJsonArray cellsToJson(const CellSet& set) const;

    int m_rows = 0;
    int m_cols = 0;
    QVector<unsigned char> m_map;
    CellSet m_primary;
    CellSet m_secondary;
    CellSet m_upperParking;
    CellSet m_lowerParking;
    QHash<quint64, int> m_manualPenalty;
    QVector<HighwayArrowMarker> m_highwayDirections;
};
