#include "panels/EnemyTuningPanel.h"
#include "EditorConsole.h"
#include "EditorContext.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

void Log(EditorContext &context, const std::string &message) {
    if (context.console != nullptr) {
        context.console->AddLog(message);
    }
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

void ReadFloat3(const json &node, float (&outValue)[3]) {
    if (!node.is_array() || node.size() < 3) {
        return;
    }
    for (int i = 0; i < 3; ++i) {
        if (node[static_cast<size_t>(i)].is_number()) {
            outValue[i] = node[static_cast<size_t>(i)].get<float>();
        }
    }
}

json Float3ToJson(const float (&value)[3]) {
    return {value[0], value[1], value[2]};
}

json TimingToJson(const EnemyTuningPanel::AttackEdit &attack) {
    const float activeStart = attack.startup;
    const float activeEnd = activeStart + attack.active;
    const float total = activeEnd + attack.recovery;
    return {{"totalTime", total},
            {"trackingEndTime", attack.trackingEndTime},
            {"activeStartTime", activeStart},
            {"activeEndTime", activeEnd},
            {"recoveryStartTime", activeEnd}};
}

json TimelineToJson(const EnemyTuningPanel::AttackEdit &attack) {
    return {{"startup", attack.startup},
            {"active", attack.active},
            {"recovery", attack.recovery},
            {"trackingEndTime", attack.trackingEndTime}};
}

json AttackParamToJson(const EnemyTuningPanel::AttackEdit &attack) {
    return {{"damage", attack.damage},
            {"knockback", attack.knockback},
            {"hitBoxSize", Float3ToJson(attack.hitBoxSize)}};
}

void ReadTimeline(const json &node, EnemyTuningPanel::AttackEdit &attack) {
    if (node.contains("timeline") && node["timeline"].is_object()) {
        const json &timeline = node["timeline"];
        ReadFloat(timeline, "startup", attack.startup);
        ReadFloat(timeline, "active", attack.active);
        ReadFloat(timeline, "recovery", attack.recovery);
        ReadFloat(timeline, "trackingEndTime", attack.trackingEndTime);
        return;
    }

    if (!node.contains("timing") || !node["timing"].is_object()) {
        return;
    }

    const json &timing = node["timing"];
    float total = attack.startup + attack.active + attack.recovery;
    float activeStart = attack.startup;
    float activeEnd = attack.startup + attack.active;
    ReadFloat(timing, "totalTime", total);
    ReadFloat(timing, "trackingEndTime", attack.trackingEndTime);
    ReadFloat(timing, "activeStartTime", activeStart);
    ReadFloat(timing, "activeEndTime", activeEnd);
    attack.startup = (std::max)(0.0f, activeStart);
    attack.active = (std::max)(0.0f, activeEnd - activeStart);
    attack.recovery = (std::max)(0.0f, total - activeEnd);
}

void ReadAttack(const json &node, EnemyTuningPanel::AttackEdit &attack) {
    const json &attackParam =
        node.contains("attack") && node["attack"].is_object() ? node["attack"]
                                                              : node;
    ReadFloat(attackParam, "damage", attack.damage);
    ReadFloat(attackParam, "knockback", attack.knockback);
    if (attackParam.contains("hitBoxSize")) {
        ReadFloat3(attackParam["hitBoxSize"], attack.hitBoxSize);
    }

    ReadFloat(node, "chargeTime", attack.chargeTime);
    ReadFloat(node, "attackForwardOffset", attack.forwardOffset);
    ReadFloat(node, "spawnForwardOffset", attack.forwardOffset);
    ReadFloat(node, "attackSideOffset", attack.sideOffset);
    ReadFloat(node, "attackHeightOffset", attack.heightOffset);
    ReadFloat(node, "spawnHeightOffset", attack.heightOffset);
    ReadFloat(node, "interval", attack.interval);
    ReadInt(node, "minCount", attack.minCount);
    ReadInt(node, "maxCount", attack.maxCount);
    ReadFloat(node, "bulletSpeed", attack.speed);
    ReadFloat(node, "speed", attack.speed);
    ReadFloat(node, "bulletLifeTime", attack.lifeTime);
    ReadFloat(node, "maxDistance", attack.maxDistance);

    if (node.contains("recoveryTime")) {
        ReadFloat(node, "recoveryTime", attack.recovery);
    }
    ReadTimeline(node, attack);
}

json MeleeAttackToJson(const EnemyTuningPanel::AttackEdit &attack,
                       bool usesSideOffset) {
    json result;
    result["attack"] = AttackParamToJson(attack);
    result["chargeTime"] = attack.chargeTime;
    if (usesSideOffset) {
        result["attackSideOffset"] = attack.sideOffset;
    } else {
        result["attackForwardOffset"] = attack.forwardOffset;
    }
    result["attackHeightOffset"] = attack.heightOffset;
    result["timing"] = TimingToJson(attack);
    result["timeline"] = TimelineToJson(attack);
    return result;
}

json ShotAttackToJson(const EnemyTuningPanel::AttackEdit &attack) {
    json result;
    result["attack"] = AttackParamToJson(attack);
    result["chargeTime"] = attack.chargeTime;
    result["recoveryTime"] = attack.recovery;
    result["interval"] = attack.interval;
    result["minCount"] = attack.minCount;
    result["maxCount"] = attack.maxCount;
    result["bulletSpeed"] = attack.speed;
    result["bulletLifeTime"] = attack.lifeTime;
    result["spawnHeightOffset"] = attack.heightOffset;
    result["timeline"] = TimelineToJson(attack);
    return result;
}

json WaveAttackToJson(const EnemyTuningPanel::AttackEdit &attack) {
    json result;
    result["attack"] = AttackParamToJson(attack);
    result["chargeTime"] = attack.chargeTime;
    result["recoveryTime"] = attack.recovery;
    result["speed"] = attack.speed;
    result["maxDistance"] = attack.maxDistance;
    result["spawnForwardOffset"] = attack.forwardOffset;
    result["spawnHeightOffset"] = attack.heightOffset;
    result["timeline"] = TimelineToJson(attack);
    return result;
}

template <typename Value>
void ClampMinMax(Value &minValue, Value &maxValue) {
    if (maxValue < minValue) {
        maxValue = minValue;
    }
}

} // namespace

void EnemyTuningPanel::EnsurePathInitialized() {
    if (pathInitialized_) {
        return;
    }
    const char *defaultPath = "resources/enemy_tuning.json";
    std::snprintf(path_.data(), path_.size(), "%s", defaultPath);
    pathInitialized_ = true;
}

void EnemyTuningPanel::Draw(EditorContext &context) {
#ifdef _DEBUG
    EnsurePathInitialized();

    ImGui::InputText("Path", path_.data(), path_.size());
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        LoadFromFile(context);
    }
    ImGui::SameLine();
    if (context.readOnly) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save")) {
        SaveToFile(context);
    }
    if (context.readOnly) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s%s", loaded_ ? "Loaded" : "Not loaded",
                        dirty_ ? " *" : "");
    if (!status_.empty()) {
        ImGui::TextWrapped("%s", status_.c_str());
    }
    if (context.readOnly) {
        ImGui::TextDisabled("Gameplay mode is read-only.");
    }

    if (context.readOnly) {
        ImGui::BeginDisabled();
    }
    DrawCore();
    DrawMovement();
    DrawAttack("Smash", smash_, true, false, false);
    DrawAttack("Sweep", sweep_, true, false, false);
    DrawAttack("Shot", shot_, false, true, false);
    DrawAttack("Wave", wave_, false, false, true);
    if (context.readOnly) {
        ImGui::EndDisabled();
    }
#else
    (void)context;
#endif
}

void EnemyTuningPanel::LoadFromFile(EditorContext &context) {
    std::ifstream file(path_.data());
    if (!file.is_open()) {
        status_ = "Failed to load enemy tuning JSON.";
        Log(context, status_ + " path=" + path_.data());
        return;
    }

    try {
        json rootJson;
        file >> rootJson;
        const json &root =
            rootJson.contains("enemy") && rootJson["enemy"].is_object()
                ? rootJson["enemy"]
                : rootJson;

        if (root.contains("core") && root["core"].is_object()) {
            const json &core = root["core"];
            ReadFloat(core, "maxHp", maxHp_);
            ReadFloat(core, "hp", maxHp_);
            ReadFloat(core, "phase2HealthRatioThreshold",
                      phase2HealthRatioThreshold_);
            ReadFloat(core, "nearAttackDistance", nearAttackDistance_);
            ReadFloat(core, "farAttackDistance", farAttackDistance_);
        }

        if (root.contains("attacks") && root["attacks"].is_object()) {
            const json &attacks = root["attacks"];
            if (attacks.contains("smash")) {
                ReadAttack(attacks["smash"], smash_);
            }
            if (attacks.contains("sweep")) {
                ReadAttack(attacks["sweep"], sweep_);
            }
            if (attacks.contains("shot")) {
                ReadAttack(attacks["shot"], shot_);
            }
            if (attacks.contains("wave")) {
                ReadAttack(attacks["wave"], wave_);
            }
        }

        if (root.contains("movement") && root["movement"].is_object()) {
            const json &movement = root["movement"];
            ReadFloat(movement, "hitReactionMoveSpeed",
                      movement_.hitReactionMoveSpeed);
            ReadFloat(movement, "chargeTurnSpeed",
                      movement_.chargeTurnSpeed);
            ReadFloat(movement, "recoveryTurnSpeed",
                      movement_.recoveryTurnSpeed);
            ReadFloat(movement, "idleTurnSpeed", movement_.idleTurnSpeed);
            ReadFloat(movement, "stagnantDistanceThreshold",
                      movement_.stagnantDistanceThreshold);
            ReadFloat(movement, "stagnantTimeThreshold",
                      movement_.stagnantTimeThreshold);
            ReadInt(movement, "stagnantWarpBonus",
                    movement_.stagnantWarpBonus);
            ReadFloat(movement, "closePressureDistance",
                      movement_.closePressureDistance);
            ReadFloat(movement, "closePressureTimeThreshold",
                      movement_.closePressureTimeThreshold);
            ReadFloat(movement, "farDistanceWarpTimeThreshold",
                      movement_.farDistanceWarpTimeThreshold);
            ReadInt(movement, "farDistanceWarpBonus",
                    movement_.farDistanceWarpBonus);
            ReadFloat(movement, "warpNearRadiusMin",
                      movement_.warpNearRadiusMin);
            ReadFloat(movement, "warpNearRadiusMax",
                      movement_.warpNearRadiusMax);
            ReadFloat(movement, "warpApproachForwardDistance",
                      movement_.warpApproachForwardDistance);
            ReadFloat(movement, "warpApproachSideDistance",
                      movement_.warpApproachSideDistance);
            ReadFloat(movement, "warpApproachLongFrontDistance",
                      movement_.warpApproachLongFrontDistance);
            ReadFloat(movement, "stalkDurationMin",
                      movement_.stalkDurationMin);
            ReadFloat(movement, "stalkDurationMax",
                      movement_.stalkDurationMax);
            ReadFloat(movement, "stalkMoveSpeed", movement_.stalkMoveSpeed);
            ReadFloat(movement, "stalkStrafeRadiusWeight",
                      movement_.stalkStrafeRadiusWeight);
            ReadFloat(movement, "stalkForwardAdjustWeight",
                      movement_.stalkForwardAdjustWeight);
            ReadFloat(movement, "stalkNearEnterChance",
                      movement_.stalkNearEnterChance);
            ReadFloat(movement, "stalkMidEnterChance",
                      movement_.stalkMidEnterChance);
            ReadInt(movement, "stalkRepeatLimit", movement_.stalkRepeatLimit);
        }

        loaded_ = true;
        dirty_ = false;
        status_ = "Enemy tuning loaded.";
        Log(context, status_);
    } catch (const json::exception &e) {
        status_ = std::string("Failed to parse enemy tuning JSON: ") + e.what();
        Log(context, status_);
    }
}

void EnemyTuningPanel::SaveToFile(EditorContext &context) {
    const std::filesystem::path path(path_.data());
    if (path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            status_ = "Failed to create enemy tuning directory.";
            Log(context, status_);
            return;
        }
    }

    ClampMinMax(movement_.warpNearRadiusMin, movement_.warpNearRadiusMax);
    ClampMinMax(movement_.stalkDurationMin, movement_.stalkDurationMax);
    ClampMinMax(shot_.minCount, shot_.maxCount);

    json root;
    root["core"] = {{"maxHp", maxHp_},
                    {"phase2HealthRatioThreshold",
                     phase2HealthRatioThreshold_},
                    {"nearAttackDistance", nearAttackDistance_},
                    {"farAttackDistance", farAttackDistance_}};
    root["attacks"]["smash"] = MeleeAttackToJson(smash_, false);
    root["attacks"]["sweep"] = MeleeAttackToJson(sweep_, true);
    root["attacks"]["shot"] = ShotAttackToJson(shot_);
    root["attacks"]["wave"] = WaveAttackToJson(wave_);
    root["attackCooldown"] = {{"shotInterval", shot_.interval},
                              {"shotRecoveryTime", shot_.recovery},
                              {"waveRecoveryTime", wave_.recovery}};
    root["movement"] = {
        {"hitReactionMoveSpeed", movement_.hitReactionMoveSpeed},
        {"chargeTurnSpeed", movement_.chargeTurnSpeed},
        {"recoveryTurnSpeed", movement_.recoveryTurnSpeed},
        {"idleTurnSpeed", movement_.idleTurnSpeed},
        {"stagnantDistanceThreshold", movement_.stagnantDistanceThreshold},
        {"stagnantTimeThreshold", movement_.stagnantTimeThreshold},
        {"stagnantWarpBonus", movement_.stagnantWarpBonus},
        {"closePressureDistance", movement_.closePressureDistance},
        {"closePressureTimeThreshold", movement_.closePressureTimeThreshold},
        {"farDistanceWarpTimeThreshold",
         movement_.farDistanceWarpTimeThreshold},
        {"farDistanceWarpBonus", movement_.farDistanceWarpBonus},
        {"warpNearRadiusMin", movement_.warpNearRadiusMin},
        {"warpNearRadiusMax", movement_.warpNearRadiusMax},
        {"warpApproachForwardDistance",
         movement_.warpApproachForwardDistance},
        {"warpApproachSideDistance", movement_.warpApproachSideDistance},
        {"warpApproachLongFrontDistance",
         movement_.warpApproachLongFrontDistance},
        {"stalkDurationMin", movement_.stalkDurationMin},
        {"stalkDurationMax", movement_.stalkDurationMax},
        {"stalkMoveSpeed", movement_.stalkMoveSpeed},
        {"stalkStrafeRadiusWeight", movement_.stalkStrafeRadiusWeight},
        {"stalkForwardAdjustWeight", movement_.stalkForwardAdjustWeight},
        {"stalkNearEnterChance", movement_.stalkNearEnterChance},
        {"stalkMidEnterChance", movement_.stalkMidEnterChance},
        {"stalkRepeatLimit", movement_.stalkRepeatLimit}};

    std::ofstream file(path);
    if (!file.is_open()) {
        status_ = "Failed to save enemy tuning JSON.";
        Log(context, status_ + " path=" + path_.data());
        return;
    }

    file << root.dump(2);
    loaded_ = true;
    dirty_ = false;
    status_ = "Enemy tuning saved.";
    Log(context, status_);
}

void EnemyTuningPanel::DrawCore() {
    if (!ImGui::CollapsingHeader("Core", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    MarkDirty(ImGui::DragFloat("HP", &maxHp_, 1.0f, 1.0f, 99999.0f));
    MarkDirty(ImGui::DragFloat("Phase 2 Ratio",
                               &phase2HealthRatioThreshold_, 0.01f, 0.0f,
                               1.0f));
    MarkDirty(ImGui::DragFloat("Near Attack Range", &nearAttackDistance_, 0.1f,
                               0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Far Attack Range", &farAttackDistance_, 0.1f,
                               0.0f, 100.0f));
}

void EnemyTuningPanel::DrawMovement() {
    if (!ImGui::CollapsingHeader("Movement")) {
        return;
    }
    MarkDirty(ImGui::DragFloat("Hit Reaction Speed",
                               &movement_.hitReactionMoveSpeed, 0.1f, 0.0f,
                               100.0f));
    MarkDirty(ImGui::DragFloat("Charge Turn Speed",
                               &movement_.chargeTurnSpeed, 0.1f, 0.0f,
                               100.0f));
    MarkDirty(ImGui::DragFloat("Recovery Turn Speed",
                               &movement_.recoveryTurnSpeed, 0.1f, 0.0f,
                               100.0f));
    MarkDirty(ImGui::DragFloat("Idle Turn Speed", &movement_.idleTurnSpeed,
                               0.1f, 0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Stagnant Distance",
                               &movement_.stagnantDistanceThreshold, 0.01f,
                               0.0f, 10.0f));
    MarkDirty(ImGui::DragFloat("Stagnant Time",
                               &movement_.stagnantTimeThreshold, 0.1f, 0.0f,
                               20.0f));
    MarkDirty(ImGui::DragInt("Stagnant Warp Bonus",
                             &movement_.stagnantWarpBonus, 1, 0, 100));
    MarkDirty(ImGui::DragFloat("Close Pressure Distance",
                               &movement_.closePressureDistance, 0.1f, 0.0f,
                               100.0f));
    MarkDirty(ImGui::DragFloat("Close Pressure Time",
                               &movement_.closePressureTimeThreshold, 0.1f,
                               0.0f, 20.0f));
    MarkDirty(ImGui::DragFloat("Far Warp Time",
                               &movement_.farDistanceWarpTimeThreshold, 0.1f,
                               0.0f, 20.0f));
    MarkDirty(ImGui::DragInt("Far Warp Bonus",
                             &movement_.farDistanceWarpBonus, 1, 0, 100));
    MarkDirty(ImGui::DragFloat("Warp Near Min", &movement_.warpNearRadiusMin,
                               0.1f, 0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Warp Near Max", &movement_.warpNearRadiusMax,
                               0.1f, 0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Warp Approach Forward",
                               &movement_.warpApproachForwardDistance, 0.1f,
                               0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Warp Approach Side",
                               &movement_.warpApproachSideDistance, 0.1f,
                               0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Warp Long Front",
                               &movement_.warpApproachLongFrontDistance, 0.1f,
                               0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Stalk Duration Min",
                               &movement_.stalkDurationMin, 0.05f, 0.0f,
                               20.0f));
    MarkDirty(ImGui::DragFloat("Stalk Duration Max",
                               &movement_.stalkDurationMax, 0.05f, 0.0f,
                               20.0f));
    MarkDirty(ImGui::DragFloat("Stalk Move Speed",
                               &movement_.stalkMoveSpeed, 0.1f, 0.0f,
                               100.0f));
    MarkDirty(ImGui::DragFloat("Stalk Strafe Weight",
                               &movement_.stalkStrafeRadiusWeight, 0.01f, 0.0f,
                               10.0f));
    MarkDirty(ImGui::DragFloat("Stalk Forward Weight",
                               &movement_.stalkForwardAdjustWeight, 0.01f,
                               0.0f, 10.0f));
    MarkDirty(ImGui::DragFloat("Stalk Near Chance",
                               &movement_.stalkNearEnterChance, 0.01f, 0.0f,
                               1.0f));
    MarkDirty(ImGui::DragFloat("Stalk Mid Chance",
                               &movement_.stalkMidEnterChance, 0.01f, 0.0f,
                               1.0f));
    MarkDirty(ImGui::DragInt("Stalk Repeat Limit",
                             &movement_.stalkRepeatLimit, 1, 0, 10));
}

void EnemyTuningPanel::DrawAttack(const char *label, AttackEdit &attack,
                                  bool melee, bool projectile, bool wave) {
    if (!ImGui::CollapsingHeader(label)) {
        return;
    }

    ImGui::PushID(label);
    MarkDirty(ImGui::DragFloat("Damage", &attack.damage, 0.1f, 0.0f,
                               9999.0f));
    MarkDirty(ImGui::DragFloat("Knockback", &attack.knockback, 0.1f, 0.0f,
                               9999.0f));
    MarkDirty(ImGui::DragFloat3("Hit Box Size", attack.hitBoxSize, 0.05f,
                                0.0f, 100.0f));
    MarkDirty(ImGui::DragFloat("Charge", &attack.chargeTime, 0.01f, 0.0f,
                               20.0f));
    MarkDirty(ImGui::DragFloat("Startup", &attack.startup, 0.01f, 0.0f,
                               20.0f));
    MarkDirty(ImGui::DragFloat("Active", &attack.active, 0.01f, 0.0f,
                               20.0f));
    MarkDirty(ImGui::DragFloat("Recovery", &attack.recovery, 0.01f, 0.0f,
                               20.0f));

    if (melee) {
        MarkDirty(ImGui::DragFloat("Tracking End",
                                   &attack.trackingEndTime, 0.01f, 0.0f,
                                   20.0f));
        MarkDirty(ImGui::DragFloat("Forward Offset",
                                   &attack.forwardOffset, 0.05f, -100.0f,
                                   100.0f));
        MarkDirty(ImGui::DragFloat("Side Offset", &attack.sideOffset, 0.05f,
                                   -100.0f, 100.0f));
        MarkDirty(ImGui::DragFloat("Height Offset", &attack.heightOffset,
                                   0.05f, -100.0f, 100.0f));
    }
    if (projectile) {
        MarkDirty(ImGui::DragFloat("Interval", &attack.interval, 0.01f, 0.0f,
                                   20.0f));
        MarkDirty(ImGui::DragInt("Min Count", &attack.minCount, 1, 0, 100));
        MarkDirty(ImGui::DragInt("Max Count", &attack.maxCount, 1, 0, 100));
        MarkDirty(ImGui::DragFloat("Speed", &attack.speed, 0.1f, 0.0f,
                                   100.0f));
        MarkDirty(ImGui::DragFloat("Life Time", &attack.lifeTime, 0.1f,
                                   0.0f, 100.0f));
        MarkDirty(ImGui::DragFloat("Spawn Height", &attack.heightOffset,
                                   0.05f, -100.0f, 100.0f));
    }
    if (wave) {
        MarkDirty(ImGui::DragFloat("Speed", &attack.speed, 0.1f, 0.0f,
                                   100.0f));
        MarkDirty(ImGui::DragFloat("Max Distance", &attack.maxDistance, 0.1f,
                                   0.0f, 100.0f));
        MarkDirty(ImGui::DragFloat("Spawn Forward", &attack.forwardOffset,
                                   0.05f, -100.0f, 100.0f));
        MarkDirty(ImGui::DragFloat("Spawn Height", &attack.heightOffset,
                                   0.05f, -100.0f, 100.0f));
    }
    ImGui::PopID();
}

void EnemyTuningPanel::MarkDirty(bool changed) {
    if (changed) {
        dirty_ = true;
    }
}
