#include "EnemyTuningPresetIO.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using json = nlohmann::json;

bool EndsWithInsensitive(const std::string &value, const std::string &suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }

    const size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char lhs = static_cast<unsigned char>(value[offset + i]);
        const unsigned char rhs = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }
    return true;
}

std::string Trim(const std::string &value) {
    const auto first =
        std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        });
    if (first == value.end()) {
        return {};
    }

    const auto last =
        std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }).base();
    return std::string(first, last);
}

std::string StripUtf8Bom(std::string value) {
    if (value.size() >= 3 && static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF) {
        value.erase(0, 3);
    }
    return value;
}

std::vector<std::string> SplitCsvLine(const std::string &line) {
    std::vector<std::string> cells;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }

        if (ch == ',' && !inQuotes) {
            cells.push_back(Trim(current));
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    cells.push_back(Trim(current));
    return cells;
}

bool ParseFloat3(const std::string &value, DirectX::XMFLOAT3 &out) {
    std::istringstream iss(value);
    return static_cast<bool>(iss >> out.x >> out.y >> out.z);
}

bool ParseFloat3Cells(const std::vector<std::string> &cells,
                      DirectX::XMFLOAT3 &outValue) {
    if (cells.size() >= 5) {
        std::istringstream x(cells[2]);
        std::istringstream y(cells[3]);
        std::istringstream z(cells[4]);
        if ((x >> outValue.x) && (y >> outValue.y) && (z >> outValue.z)) {
            return true;
        }
    }
    if (cells.size() >= 3) {
        return ParseFloat3(cells[2], outValue);
    }
    return false;
}

template <typename T> bool ParseCell(const std::vector<std::string> &cells,
                                     size_t index, T &outValue) {
    if (index >= cells.size()) {
        return false;
    }
    std::istringstream iss(cells[index]);
    T parsed{};
    if (!(iss >> parsed)) {
        return false;
    }
    outValue = parsed;
    return true;
}

bool LoadLegacyText(const std::string &path, EnemyTuningPreset &p) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return false;
    }

    auto readOptional = [&ifs](auto &value) {
        using ValueType = std::decay_t<decltype(value)>;
        ValueType temp{};
        if (ifs >> temp) {
            value = temp;
            return;
        }
        ifs.clear();
    };

    ifs >> p.nearAttackDistance;
    ifs >> p.farAttackDistance;

    ifs >> p.smash.damage >> p.smash.knockback >> p.smash.hitBoxSize.x >>
        p.smash.hitBoxSize.y >> p.smash.hitBoxSize.z;
    ifs >> p.smashChargeTime >> p.smashAttackForwardOffset >>
        p.smashAttackHeightOffset;
    ifs >> p.smashTiming.totalTime >> p.smashTiming.trackingEndTime >>
        p.smashTiming.activeStartTime >> p.smashTiming.activeEndTime >>
        p.smashTiming.recoveryStartTime;

    ifs >> p.sweep.damage >> p.sweep.knockback >> p.sweep.hitBoxSize.x >>
        p.sweep.hitBoxSize.y >> p.sweep.hitBoxSize.z;
    ifs >> p.sweepChargeTime >> p.sweepAttackSideOffset >>
        p.sweepAttackHeightOffset;
    ifs >> p.sweepTiming.totalTime >> p.sweepTiming.trackingEndTime >>
        p.sweepTiming.activeStartTime >> p.sweepTiming.activeEndTime >>
        p.sweepTiming.recoveryStartTime;

    ifs >> p.bullet.damage >> p.bullet.knockback >> p.bullet.hitBoxSize.x >>
        p.bullet.hitBoxSize.y >> p.bullet.hitBoxSize.z;
    ifs >> p.shotChargeTime >> p.shotRecoveryTime >> p.shotInterval >>
        p.shotMinCount >> p.shotMaxCount >> p.bulletSpeed >> p.bulletLifeTime >>
        p.bulletSpawnHeightOffset;

    ifs >> p.wave.damage >> p.wave.knockback >> p.wave.hitBoxSize.x >>
        p.wave.hitBoxSize.y >> p.wave.hitBoxSize.z;
    ifs >> p.waveChargeTime >> p.waveRecoveryTime >> p.waveSpeed >>
        p.waveMaxDistance >> p.waveSpawnForwardOffset >>
        p.waveSpawnHeightOffset;

    readOptional(p.warpApproachChainMaxSteps);
    readOptional(p.warpEscapeChainMaxSteps);
    readOptional(p.approachChainContinueDistance);
    readOptional(p.escapeChainContinueDistance);

    readOptional(p.sweepWarpSmashMaxDistance);
    readOptional(p.sweepWarpSmashChance);
    readOptional(p.waveWarpSmashMinDistance);
    readOptional(p.waveWarpSmashChance);

    readOptional(p.enemyMaxHp);
    readOptional(p.phase2HealthRatioThreshold);

    readOptional(p.warpStartTime);
    readOptional(p.warpMoveTime);
    readOptional(p.warpEndTime);

    return true;
}

bool LoadNumericCsv(const std::string &path, EnemyTuningPreset &p) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return false;
    }

    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(ifs, line)) {
        line = StripUtf8Bom(line);
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        rows.push_back(SplitCsvLine(trimmed));
    }

    if (rows.size() < 16) {
        return false;
    }

    ParseCell(rows[0], 0, p.nearAttackDistance);
    ParseCell(rows[1], 0, p.farAttackDistance);

    ParseCell(rows[2], 0, p.smash.damage);
    ParseCell(rows[2], 1, p.smash.knockback);
    ParseFloat3Cells(rows[2], p.smash.hitBoxSize);
    ParseCell(rows[3], 0, p.smashChargeTime);
    ParseCell(rows[3], 1, p.smashAttackForwardOffset);
    ParseCell(rows[3], 2, p.smashAttackHeightOffset);
    ParseCell(rows[4], 0, p.smashTiming.totalTime);
    ParseCell(rows[4], 1, p.smashTiming.trackingEndTime);
    ParseCell(rows[4], 2, p.smashTiming.activeStartTime);
    ParseCell(rows[4], 3, p.smashTiming.activeEndTime);
    ParseCell(rows[4], 4, p.smashTiming.recoveryStartTime);

    ParseCell(rows[5], 0, p.sweep.damage);
    ParseCell(rows[5], 1, p.sweep.knockback);
    ParseFloat3Cells(rows[5], p.sweep.hitBoxSize);
    ParseCell(rows[6], 0, p.sweepChargeTime);
    ParseCell(rows[6], 1, p.sweepAttackSideOffset);
    ParseCell(rows[6], 2, p.sweepAttackHeightOffset);
    ParseCell(rows[7], 0, p.sweepTiming.totalTime);
    ParseCell(rows[7], 1, p.sweepTiming.trackingEndTime);
    ParseCell(rows[7], 2, p.sweepTiming.activeStartTime);
    ParseCell(rows[7], 3, p.sweepTiming.activeEndTime);
    ParseCell(rows[7], 4, p.sweepTiming.recoveryStartTime);

    ParseCell(rows[8], 0, p.bullet.damage);
    ParseCell(rows[8], 1, p.bullet.knockback);
    ParseFloat3Cells(rows[8], p.bullet.hitBoxSize);
    ParseCell(rows[9], 0, p.shotChargeTime);
    ParseCell(rows[9], 1, p.shotRecoveryTime);
    ParseCell(rows[9], 2, p.shotInterval);
    ParseCell(rows[9], 3, p.shotMinCount);
    ParseCell(rows[9], 4, p.shotMaxCount);
    ParseCell(rows[9], 5, p.bulletSpeed);
    ParseCell(rows[9], 6, p.bulletLifeTime);
    ParseCell(rows[9], 7, p.bulletSpawnHeightOffset);

    ParseCell(rows[10], 0, p.wave.damage);
    ParseCell(rows[10], 1, p.wave.knockback);
    ParseFloat3Cells(rows[10], p.wave.hitBoxSize);
    ParseCell(rows[11], 0, p.waveChargeTime);
    ParseCell(rows[11], 1, p.waveRecoveryTime);
    ParseCell(rows[11], 2, p.waveSpeed);
    ParseCell(rows[11], 3, p.waveMaxDistance);
    ParseCell(rows[11], 4, p.waveSpawnForwardOffset);
    ParseCell(rows[11], 5, p.waveSpawnHeightOffset);

    ParseCell(rows[12], 0, p.warpApproachChainMaxSteps);
    ParseCell(rows[12], 1, p.warpEscapeChainMaxSteps);
    ParseCell(rows[12], 2, p.approachChainContinueDistance);
    ParseCell(rows[12], 3, p.escapeChainContinueDistance);

    ParseCell(rows[13], 0, p.sweepWarpSmashMaxDistance);
    ParseCell(rows[13], 1, p.sweepWarpSmashChance);
    ParseCell(rows[13], 2, p.waveWarpSmashMinDistance);
    ParseCell(rows[13], 3, p.waveWarpSmashChance);

    ParseCell(rows[14], 0, p.enemyMaxHp);
    ParseCell(rows[14], 1, p.phase2HealthRatioThreshold);

    ParseCell(rows[15], 0, p.warpStartTime);
    ParseCell(rows[15], 1, p.warpMoveTime);
    ParseCell(rows[15], 2, p.warpEndTime);
    return true;
}

void ReadFloat(const json &node, const char *key, float &outValue) {
    if (node.contains(key) && node[key].is_number()) {
        outValue = node[key].get<float>();
    }
}

void ReadInt(const json &node, const char *key, int &outValue) {
    if (node.contains(key) && node[key].is_number_integer()) {
        outValue = node[key].get<int>();
    }
}

void ReadFloat3(const json &node, DirectX::XMFLOAT3 &outValue) {
    if (!node.is_array() || node.size() < 3) {
        return;
    }
    outValue = {node[0].get<float>(), node[1].get<float>(),
                node[2].get<float>()};
}

void ReadAttackParam(const json &node, AttackParamPreset &outValue) {
    ReadFloat(node, "damage", outValue.damage);
    ReadFloat(node, "knockback", outValue.knockback);
    if (node.contains("hitBoxSize")) {
        ReadFloat3(node["hitBoxSize"], outValue.hitBoxSize);
    }
}

void ReadTiming(const json &node, AttackTimingParamPreset &outValue) {
    ReadFloat(node, "totalTime", outValue.totalTime);
    ReadFloat(node, "trackingEndTime", outValue.trackingEndTime);
    ReadFloat(node, "activeStartTime", outValue.activeStartTime);
    ReadFloat(node, "activeEndTime", outValue.activeEndTime);
    ReadFloat(node, "recoveryStartTime", outValue.recoveryStartTime);
}

void ReadAttackRange(const json &node, EnemyTuningPreset &outValue) {
    if (node.is_number()) {
        outValue.nearAttackDistance = node.get<float>();
        outValue.farAttackDistance = outValue.nearAttackDistance;
        return;
    }
    if (node.is_array() && node.size() >= 2) {
        outValue.nearAttackDistance = node[0].get<float>();
        outValue.farAttackDistance = node[1].get<float>();
        return;
    }
    if (node.is_object()) {
        ReadFloat(node, "near", outValue.nearAttackDistance);
        ReadFloat(node, "far", outValue.farAttackDistance);
    }
}

void ReadMovement(const json &node, EnemyMovementPreset &outValue) {
    ReadFloat(node, "hitReactionMoveSpeed", outValue.hitReactionMoveSpeed);

    ReadFloat(node, "chargeTurnSpeed", outValue.chargeTurnSpeed);
    ReadFloat(node, "recoveryTurnSpeed", outValue.recoveryTurnSpeed);
    ReadFloat(node, "idleTurnSpeed", outValue.idleTurnSpeed);

    ReadFloat(node, "stagnantDistanceThreshold",
              outValue.stagnantDistanceThreshold);
    ReadFloat(node, "stagnantTimeThreshold", outValue.stagnantTimeThreshold);
    ReadInt(node, "stagnantWarpBonus", outValue.stagnantWarpBonus);

    ReadFloat(node, "closePressureDistance", outValue.closePressureDistance);
    ReadFloat(node, "closePressureTimeThreshold",
              outValue.closePressureTimeThreshold);

    ReadFloat(node, "farDistanceWarpTimeThreshold",
              outValue.farDistanceWarpTimeThreshold);
    ReadInt(node, "farDistanceWarpBonus", outValue.farDistanceWarpBonus);

    ReadFloat(node, "warpNearRadiusMin", outValue.warpNearRadiusMin);
    ReadFloat(node, "warpNearRadiusMax", outValue.warpNearRadiusMax);
    ReadFloat(node, "warpApproachForwardDistance",
              outValue.warpApproachForwardDistance);
    ReadFloat(node, "warpApproachSideDistance",
              outValue.warpApproachSideDistance);
    ReadFloat(node, "warpApproachLongFrontDistance",
              outValue.warpApproachLongFrontDistance);

    ReadFloat(node, "stalkDurationMin", outValue.stalkDurationMin);
    ReadFloat(node, "stalkDurationMax", outValue.stalkDurationMax);
    ReadFloat(node, "stalkMoveSpeed", outValue.stalkMoveSpeed);
    ReadFloat(node, "stalkStrafeRadiusWeight",
              outValue.stalkStrafeRadiusWeight);
    ReadFloat(node, "stalkForwardAdjustWeight",
              outValue.stalkForwardAdjustWeight);
    ReadFloat(node, "stalkNearEnterChance", outValue.stalkNearEnterChance);
    ReadFloat(node, "stalkMidEnterChance", outValue.stalkMidEnterChance);
    ReadInt(node, "stalkRepeatLimit", outValue.stalkRepeatLimit);
}

void ReadSmash(const json &node, EnemyTuningPreset &outValue) {
    if (node.contains("attack")) {
        ReadAttackParam(node["attack"], outValue.smash);
    } else {
        ReadAttackParam(node, outValue.smash);
    }
    ReadFloat(node, "chargeTime", outValue.smashChargeTime);
    ReadFloat(node, "attackForwardOffset", outValue.smashAttackForwardOffset);
    ReadFloat(node, "attackHeightOffset", outValue.smashAttackHeightOffset);
    if (node.contains("timing")) {
        ReadTiming(node["timing"], outValue.smashTiming);
    }
}

void ReadSweep(const json &node, EnemyTuningPreset &outValue) {
    if (node.contains("attack")) {
        ReadAttackParam(node["attack"], outValue.sweep);
    } else {
        ReadAttackParam(node, outValue.sweep);
    }
    ReadFloat(node, "chargeTime", outValue.sweepChargeTime);
    ReadFloat(node, "attackSideOffset", outValue.sweepAttackSideOffset);
    ReadFloat(node, "attackHeightOffset", outValue.sweepAttackHeightOffset);
    if (node.contains("timing")) {
        ReadTiming(node["timing"], outValue.sweepTiming);
    }
}

void ReadShot(const json &node, EnemyTuningPreset &outValue) {
    if (node.contains("attack")) {
        ReadAttackParam(node["attack"], outValue.bullet);
    } else {
        ReadAttackParam(node, outValue.bullet);
    }
    ReadFloat(node, "chargeTime", outValue.shotChargeTime);
    ReadFloat(node, "recoveryTime", outValue.shotRecoveryTime);
    ReadFloat(node, "interval", outValue.shotInterval);
    ReadInt(node, "minCount", outValue.shotMinCount);
    ReadInt(node, "maxCount", outValue.shotMaxCount);
    ReadFloat(node, "bulletSpeed", outValue.bulletSpeed);
    ReadFloat(node, "bulletLifeTime", outValue.bulletLifeTime);
    ReadFloat(node, "spawnHeightOffset", outValue.bulletSpawnHeightOffset);
}

void ReadWave(const json &node, EnemyTuningPreset &outValue) {
    if (node.contains("attack")) {
        ReadAttackParam(node["attack"], outValue.wave);
    } else {
        ReadAttackParam(node, outValue.wave);
    }
    ReadFloat(node, "chargeTime", outValue.waveChargeTime);
    ReadFloat(node, "recoveryTime", outValue.waveRecoveryTime);
    ReadFloat(node, "speed", outValue.waveSpeed);
    ReadFloat(node, "maxDistance", outValue.waveMaxDistance);
    ReadFloat(node, "spawnForwardOffset", outValue.waveSpawnForwardOffset);
    ReadFloat(node, "spawnHeightOffset", outValue.waveSpawnHeightOffset);
}

bool LoadJson(const std::string &path, EnemyTuningPreset &p) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return false;
    }

    json rootJson;
    try {
        ifs >> rootJson;
    } catch (const json::exception &) {
        return false;
    }

    const json &root = rootJson.contains("enemy") ? rootJson["enemy"] : rootJson;

    ReadFloat(root, "HP", p.enemyMaxHp);
    ReadFloat(root, "hp", p.enemyMaxHp);
    ReadFloat(root, "maxHp", p.enemyMaxHp);

    if (root.contains("attackRange")) {
        ReadAttackRange(root["attackRange"], p);
    }

    if (root.contains("core")) {
        const json &core = root["core"];
        ReadFloat(core, "maxHp", p.enemyMaxHp);
        ReadFloat(core, "hp", p.enemyMaxHp);
        ReadFloat(core, "phase2HealthRatioThreshold",
                  p.phase2HealthRatioThreshold);
        ReadFloat(core, "nearAttackDistance", p.nearAttackDistance);
        ReadFloat(core, "farAttackDistance", p.farAttackDistance);
        if (core.contains("attackRange")) {
            ReadAttackRange(core["attackRange"], p);
        }
    }

    if (root.contains("attacks")) {
        const json &attacks = root["attacks"];
        if (attacks.contains("smash")) {
            ReadSmash(attacks["smash"], p);
        }
        if (attacks.contains("sweep")) {
            ReadSweep(attacks["sweep"], p);
        }
        if (attacks.contains("shot")) {
            ReadShot(attacks["shot"], p);
        }
        if (attacks.contains("bullet")) {
            ReadShot(attacks["bullet"], p);
        }
        if (attacks.contains("wave")) {
            ReadWave(attacks["wave"], p);
        }
    }

    if (root.contains("attackCooldown") && root["attackCooldown"].is_object()) {
        const json &cooldown = root["attackCooldown"];
        ReadFloat(cooldown, "shotInterval", p.shotInterval);
        ReadFloat(cooldown, "shotRecoveryTime", p.shotRecoveryTime);
        ReadFloat(cooldown, "waveRecoveryTime", p.waveRecoveryTime);
    }

    if (root.contains("warp")) {
        const json &warp = root["warp"];
        ReadFloat(warp, "startTime", p.warpStartTime);
        ReadFloat(warp, "moveTime", p.warpMoveTime);
        ReadFloat(warp, "endTime", p.warpEndTime);
    }

    if (root.contains("chain")) {
        const json &chain = root["chain"];
        ReadInt(chain, "warpApproachMaxSteps", p.warpApproachChainMaxSteps);
        ReadInt(chain, "warpEscapeMaxSteps", p.warpEscapeChainMaxSteps);
        ReadFloat(chain, "approachContinueDistance",
                  p.approachChainContinueDistance);
        ReadFloat(chain, "escapeContinueDistance",
                  p.escapeChainContinueDistance);
        ReadFloat(chain, "sweepWarpSmashMaxDistance",
                  p.sweepWarpSmashMaxDistance);
        ReadFloat(chain, "sweepWarpSmashChance", p.sweepWarpSmashChance);
        ReadFloat(chain, "waveWarpSmashMinDistance",
                  p.waveWarpSmashMinDistance);
        ReadFloat(chain, "waveWarpSmashChance", p.waveWarpSmashChance);
    }

    if (root.contains("movement")) {
        ReadMovement(root["movement"], p.movement);
    }

    if (root.contains("speed") && root["speed"].is_object()) {
        const json &speed = root["speed"];
        ReadFloat(speed, "bullet", p.bulletSpeed);
        ReadFloat(speed, "wave", p.waveSpeed);
        ReadFloat(speed, "stalk", p.movement.stalkMoveSpeed);
        ReadFloat(speed, "turnIdle", p.movement.idleTurnSpeed);
        ReadFloat(speed, "turnCharge", p.movement.chargeTurnSpeed);
        ReadFloat(speed, "turnRecovery", p.movement.recoveryTurnSpeed);
    }

    return true;
}

json Float3ToJson(const DirectX::XMFLOAT3 &value) {
    return {value.x, value.y, value.z};
}

json AttackParamToJson(const AttackParamPreset &value) {
    return {{"damage", value.damage},
            {"knockback", value.knockback},
            {"hitBoxSize", Float3ToJson(value.hitBoxSize)}};
}

json TimingToJson(const AttackTimingParamPreset &value) {
    return {{"totalTime", value.totalTime},
            {"trackingEndTime", value.trackingEndTime},
            {"activeStartTime", value.activeStartTime},
            {"activeEndTime", value.activeEndTime},
            {"recoveryStartTime", value.recoveryStartTime}};
}

json MovementToJson(const EnemyMovementPreset &value) {
    return {{"hitReactionMoveSpeed", value.hitReactionMoveSpeed},
            {"chargeTurnSpeed", value.chargeTurnSpeed},
            {"recoveryTurnSpeed", value.recoveryTurnSpeed},
            {"idleTurnSpeed", value.idleTurnSpeed},
            {"stagnantDistanceThreshold", value.stagnantDistanceThreshold},
            {"stagnantTimeThreshold", value.stagnantTimeThreshold},
            {"stagnantWarpBonus", value.stagnantWarpBonus},
            {"closePressureDistance", value.closePressureDistance},
            {"closePressureTimeThreshold", value.closePressureTimeThreshold},
            {"farDistanceWarpTimeThreshold", value.farDistanceWarpTimeThreshold},
            {"farDistanceWarpBonus", value.farDistanceWarpBonus},
            {"warpNearRadiusMin", value.warpNearRadiusMin},
            {"warpNearRadiusMax", value.warpNearRadiusMax},
            {"warpApproachForwardDistance", value.warpApproachForwardDistance},
            {"warpApproachSideDistance", value.warpApproachSideDistance},
            {"warpApproachLongFrontDistance",
             value.warpApproachLongFrontDistance},
            {"stalkDurationMin", value.stalkDurationMin},
            {"stalkDurationMax", value.stalkDurationMax},
            {"stalkMoveSpeed", value.stalkMoveSpeed},
            {"stalkStrafeRadiusWeight", value.stalkStrafeRadiusWeight},
            {"stalkForwardAdjustWeight", value.stalkForwardAdjustWeight},
            {"stalkNearEnterChance", value.stalkNearEnterChance},
            {"stalkMidEnterChance", value.stalkMidEnterChance},
            {"stalkRepeatLimit", value.stalkRepeatLimit}};
}

json PresetToJson(const EnemyTuningPreset &p) {
    json root;
    root["core"] = {{"maxHp", p.enemyMaxHp},
                    {"phase2HealthRatioThreshold",
                     p.phase2HealthRatioThreshold},
                    {"nearAttackDistance", p.nearAttackDistance},
                    {"farAttackDistance", p.farAttackDistance}};
    root["attacks"]["smash"] = {
        {"attack", AttackParamToJson(p.smash)},
        {"chargeTime", p.smashChargeTime},
        {"attackForwardOffset", p.smashAttackForwardOffset},
        {"attackHeightOffset", p.smashAttackHeightOffset},
        {"timing", TimingToJson(p.smashTiming)}};
    root["attacks"]["sweep"] = {
        {"attack", AttackParamToJson(p.sweep)},
        {"chargeTime", p.sweepChargeTime},
        {"attackSideOffset", p.sweepAttackSideOffset},
        {"attackHeightOffset", p.sweepAttackHeightOffset},
        {"timing", TimingToJson(p.sweepTiming)}};
    root["attacks"]["shot"] = {
        {"attack", AttackParamToJson(p.bullet)},
        {"chargeTime", p.shotChargeTime},
        {"recoveryTime", p.shotRecoveryTime},
        {"interval", p.shotInterval},
        {"minCount", p.shotMinCount},
        {"maxCount", p.shotMaxCount},
        {"bulletSpeed", p.bulletSpeed},
        {"bulletLifeTime", p.bulletLifeTime},
        {"spawnHeightOffset", p.bulletSpawnHeightOffset}};
    root["attacks"]["wave"] = {
        {"attack", AttackParamToJson(p.wave)},
        {"chargeTime", p.waveChargeTime},
        {"recoveryTime", p.waveRecoveryTime},
        {"speed", p.waveSpeed},
        {"maxDistance", p.waveMaxDistance},
        {"spawnForwardOffset", p.waveSpawnForwardOffset},
        {"spawnHeightOffset", p.waveSpawnHeightOffset}};
    root["warp"] = {{"startTime", p.warpStartTime},
                    {"moveTime", p.warpMoveTime},
                    {"endTime", p.warpEndTime}};
    root["chain"] = {{"warpApproachMaxSteps", p.warpApproachChainMaxSteps},
                     {"warpEscapeMaxSteps", p.warpEscapeChainMaxSteps},
                     {"approachContinueDistance",
                      p.approachChainContinueDistance},
                     {"escapeContinueDistance", p.escapeChainContinueDistance},
                     {"sweepWarpSmashMaxDistance",
                      p.sweepWarpSmashMaxDistance},
                     {"sweepWarpSmashChance", p.sweepWarpSmashChance},
                     {"waveWarpSmashMinDistance", p.waveWarpSmashMinDistance},
                     {"waveWarpSmashChance", p.waveWarpSmashChance}};
    root["movement"] = MovementToJson(p.movement);
    return root;
}

} // namespace

bool EnemyTuningPresetIO::Save(const std::string &path,
                               const EnemyTuningPreset &p) {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        return false;
    }

    if (EndsWithInsensitive(path, ".json")) {
        ofs << PresetToJson(p).dump(2);
        return true;
    }

    if (EndsWithInsensitive(path, ".csv")) {
        ofs << p.nearAttackDistance << "\n";
        ofs << p.farAttackDistance << "\n";
        ofs << p.smash.damage << "," << p.smash.knockback << ","
            << p.smash.hitBoxSize.x << "," << p.smash.hitBoxSize.y << ","
            << p.smash.hitBoxSize.z << "\n";
        ofs << p.smashChargeTime << "," << p.smashAttackForwardOffset << ","
            << p.smashAttackHeightOffset << "\n";
        ofs << p.smashTiming.totalTime << "," << p.smashTiming.trackingEndTime
            << "," << p.smashTiming.activeStartTime << ","
            << p.smashTiming.activeEndTime << ","
            << p.smashTiming.recoveryStartTime << "\n";
        ofs << p.sweep.damage << "," << p.sweep.knockback << ","
            << p.sweep.hitBoxSize.x << "," << p.sweep.hitBoxSize.y << ","
            << p.sweep.hitBoxSize.z << "\n";
        ofs << p.sweepChargeTime << "," << p.sweepAttackSideOffset << ","
            << p.sweepAttackHeightOffset << "\n";
        ofs << p.sweepTiming.totalTime << "," << p.sweepTiming.trackingEndTime
            << "," << p.sweepTiming.activeStartTime << ","
            << p.sweepTiming.activeEndTime << ","
            << p.sweepTiming.recoveryStartTime << "\n";
        ofs << p.bullet.damage << "," << p.bullet.knockback << ","
            << p.bullet.hitBoxSize.x << "," << p.bullet.hitBoxSize.y << ","
            << p.bullet.hitBoxSize.z << "\n";
        ofs << p.shotChargeTime << "," << p.shotRecoveryTime << ","
            << p.shotInterval << "," << p.shotMinCount << "," << p.shotMaxCount
            << "," << p.bulletSpeed << "," << p.bulletLifeTime << ","
            << p.bulletSpawnHeightOffset << "\n";
        ofs << p.wave.damage << "," << p.wave.knockback << ","
            << p.wave.hitBoxSize.x << "," << p.wave.hitBoxSize.y << ","
            << p.wave.hitBoxSize.z << "\n";
        ofs << p.waveChargeTime << "," << p.waveRecoveryTime << ","
            << p.waveSpeed << "," << p.waveMaxDistance << ","
            << p.waveSpawnForwardOffset << "," << p.waveSpawnHeightOffset
            << "\n";
        ofs << p.warpApproachChainMaxSteps << "," << p.warpEscapeChainMaxSteps
            << "," << p.approachChainContinueDistance << ","
            << p.escapeChainContinueDistance << "\n";
        ofs << p.sweepWarpSmashMaxDistance << "," << p.sweepWarpSmashChance
            << "," << p.waveWarpSmashMinDistance << "," << p.waveWarpSmashChance
            << "\n";
        ofs << p.enemyMaxHp << "," << p.phase2HealthRatioThreshold << "\n";
        ofs << p.warpStartTime << "," << p.warpMoveTime << "," << p.warpEndTime
            << "\n";
        return true;
    }

    ofs << p.nearAttackDistance << "\n";
    ofs << p.farAttackDistance << "\n";

    ofs << p.smash.damage << " " << p.smash.knockback << " "
        << p.smash.hitBoxSize.x << " " << p.smash.hitBoxSize.y << " "
        << p.smash.hitBoxSize.z << "\n";
    ofs << p.smashChargeTime << " " << p.smashAttackForwardOffset << " "
        << p.smashAttackHeightOffset << "\n";
    ofs << p.smashTiming.totalTime << " " << p.smashTiming.trackingEndTime
        << " " << p.smashTiming.activeStartTime << " "
        << p.smashTiming.activeEndTime << " " << p.smashTiming.recoveryStartTime
        << "\n";

    ofs << p.sweep.damage << " " << p.sweep.knockback << " "
        << p.sweep.hitBoxSize.x << " " << p.sweep.hitBoxSize.y << " "
        << p.sweep.hitBoxSize.z << "\n";
    ofs << p.sweepChargeTime << " " << p.sweepAttackSideOffset << " "
        << p.sweepAttackHeightOffset << "\n";
    ofs << p.sweepTiming.totalTime << " " << p.sweepTiming.trackingEndTime
        << " " << p.sweepTiming.activeStartTime << " "
        << p.sweepTiming.activeEndTime << " " << p.sweepTiming.recoveryStartTime
        << "\n";

    ofs << p.bullet.damage << " " << p.bullet.knockback << " "
        << p.bullet.hitBoxSize.x << " " << p.bullet.hitBoxSize.y << " "
        << p.bullet.hitBoxSize.z << "\n";
    ofs << p.shotChargeTime << " " << p.shotRecoveryTime << " "
        << p.shotInterval << " " << p.shotMinCount << " " << p.shotMaxCount
        << " " << p.bulletSpeed << " " << p.bulletLifeTime << " "
        << p.bulletSpawnHeightOffset << "\n";

    ofs << p.wave.damage << " " << p.wave.knockback << " "
        << p.wave.hitBoxSize.x << " " << p.wave.hitBoxSize.y << " "
        << p.wave.hitBoxSize.z << "\n";
    ofs << p.waveChargeTime << " " << p.waveRecoveryTime << " " << p.waveSpeed
        << " " << p.waveMaxDistance << " " << p.waveSpawnForwardOffset << " "
        << p.waveSpawnHeightOffset << "\n";

    ofs << p.warpApproachChainMaxSteps << " " << p.warpEscapeChainMaxSteps << " "
        << p.approachChainContinueDistance << " "
        << p.escapeChainContinueDistance << "\n";
    ofs << p.sweepWarpSmashMaxDistance << " " << p.sweepWarpSmashChance << " "
        << p.waveWarpSmashMinDistance << " " << p.waveWarpSmashChance << "\n";
    ofs << p.enemyMaxHp << " " << p.phase2HealthRatioThreshold << "\n";
    ofs << p.warpStartTime << " " << p.warpMoveTime << " " << p.warpEndTime
        << "\n";

    return true;
}

bool EnemyTuningPresetIO::Load(const std::string &path, EnemyTuningPreset &p) {
    if (EndsWithInsensitive(path, ".json")) {
        return LoadJson(path, p);
    }
    if (EndsWithInsensitive(path, ".csv")) {
        return LoadNumericCsv(path, p);
    }
    return LoadLegacyText(path, p);
}
