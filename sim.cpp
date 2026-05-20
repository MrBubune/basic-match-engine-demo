#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

// ==========================================
// DATA STRUCTURES
// ==========================================

struct PlayerStats {
    int goals = 0;
    int shots_on_target = 0;
    int shots_total = 0;
    int passes_attempted = 0;
    int passes_completed = 0;
    int tackles_attempted = 0;
    int tackles_won = 0;
    int dribbles_attempted = 0;
    int dribbles_won = 0;
    int interceptions = 0;
    int fouls = 0;
    int yellow_cards = 0;
    int red_cards = 0;
    int saves = 0;
};

struct Player {
    int id;
    std::string name, position;

    // Attributes
    int finishing, heading, attacking_pos, volleys, penalties;
    int passing, vision, dribbling, first_touch, crossing;
    int tackling, marking, interceptions, strength, aggression;
    int reflexes, positioning, composure, anticipation;
    int pace, stamina, decisions, work_rate;

    PlayerStats stats;
    double current_stamina = 100.0;
    double match_rating = 6.7;

    double get_eff(int attr) const {
        double fatigue_mult = current_stamina / 100.0;
        return attr * (0.8 + (0.2 * fatigue_mult));
    }

    void update_rating() {
        double score =
            6.7 +
            (stats.goals * 1.5) +
            (stats.saves * 0.4) +
            (stats.tackles_won * 0.3) +
            (stats.passes_completed * 0.01);

        double penalty =
            (stats.passes_attempted - stats.passes_completed) * 0.05 +
            ((stats.shots_total - stats.shots_on_target) * 0.1) +
            (stats.fouls * 0.1);

        match_rating = std::clamp(score - penalty, 1.0, 10.0);
    }
};

struct Team {
    int id;
    std::string name;
    int reputation;

    std::vector<Player> squad;

    // Match stats
    int goals = 0;
    int shots = 0;
    int tackles = 0;
    int fouls = 0;
    int yellow = 0;
    int red = 0;
    int pass_att = 0;
    int pass_comp = 0;
    int possession_ticks = 0;
    int saves = 0;
};

struct MatchState {
    Team home;
    Team away;
    Team* possession = nullptr;
    int minute = 0;
};

// ==========================================
// HELPERS
// ==========================================

void trim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> result;
    std::string cell;
    bool in_quotes = false;

    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        }
        else if (c == ',' && !in_quotes) {
            result.push_back(cell);
            cell.clear();
        }
        else {
            cell.push_back(c);
        }
    }

    result.push_back(cell);
    return result;
}

int to_int(const std::string& s) {
    try {
        return std::stoi(s);
    }
    catch (...) {
        std::cerr << "Invalid integer field: [" << s << "]\n";
        throw;
    }
}

// ==========================================
// LOAD TEAM
// ==========================================

Team load_team_from_csv(int team_id) {
    Team team;
    bool found = false;

    // --------------------------
    // Load teams.csv
    // --------------------------

    std::ifstream teams_file("database/teams.csv");

    if (!teams_file.is_open()) {
        throw std::runtime_error("Could not open database/teams.csv");
    }

    std::string line;

    std::getline(teams_file, line); // skip header

    while (std::getline(teams_file, line)) {
        trim_cr(line);

        auto row = split_csv(line);

        if (row.size() < 3)
            continue;

        if (to_int(row[0]) == team_id) {
            team.id = to_int(row[0]);
            team.name = row[1];
            team.reputation = to_int(row[2]);
            found = true;
            break;
        }
    }

    teams_file.close();

    if (!found) {
        throw std::runtime_error("Team ID not found.");
    }

    // --------------------------
    // Load players.csv
    // --------------------------

    std::ifstream players_file("database/players.csv");

    if (!players_file.is_open()) {
        throw std::runtime_error("Could not open database/players.csv");
    }

    std::getline(players_file, line); // skip header

    while (std::getline(players_file, line)) {
        trim_cr(line);

        auto row = split_csv(line);

        if (row.size() < 29)
            continue;

        if (row[0].empty())
            continue;

        // TeamID column
        if (to_int(row[1]) == team_id) {

            Player p;

            p.id = to_int(row[0]);
            p.name = row[2];
            p.position = row[5];

            // Attacking
            p.finishing = to_int(row[6]);
            p.heading = to_int(row[7]);
            p.attacking_pos = to_int(row[8]);
            p.volleys = to_int(row[9]);
            p.penalties = to_int(row[10]);

            // Midfield
            p.passing = to_int(row[11]);
            p.vision = to_int(row[12]);
            p.dribbling = to_int(row[13]);
            p.first_touch = to_int(row[14]);
            p.crossing = to_int(row[15]);

            // Defending
            p.tackling = to_int(row[16]);
            p.marking = to_int(row[17]);
            p.interceptions = to_int(row[18]);
            p.strength = to_int(row[19]);
            p.aggression = to_int(row[20]);

            // Mental / Physical / GK
            p.reflexes = to_int(row[21]);
            p.positioning = to_int(row[22]);
            p.composure = to_int(row[23]);
            p.anticipation = to_int(row[24]);
            p.pace = to_int(row[25]);
            p.stamina = to_int(row[26]);
            p.decisions = to_int(row[27]);
            p.work_rate = to_int(row[28]);

            team.squad.push_back(p);
        }
    }

    players_file.close();

    if (team.squad.size() < 11) {
        std::cerr << "Warning: Team "
                  << team.name
                  << " only loaded "
                  << team.squad.size()
                  << " players.\n";
    }

    return team;
}

// ==========================================
// MATH CORE
// ==========================================

double sigmoid_prob(double attacker, double defender, double k = 0.06) {
    double diff = attacker - defender;
    return 1.0 / (1.0 + std::exp(-k * diff));
}

// ==========================================
// MATCH ENGINE
// ==========================================

void resolve_event(MatchState& ms, std::mt19937& gen) {

    std::uniform_real_distribution<double> roll(0.0, 1.0);

    Team* atk_team = ms.possession;
    Team* def_team =
        (ms.possession->id == ms.home.id)
            ? &ms.away
            : &ms.home;

    atk_team->possession_ticks++;

    std::uniform_int_distribution<int> atk_pick(
        0,
        (int)atk_team->squad.size() - 1
    );

    std::uniform_int_distribution<int> def_pick(
        0,
        (int)def_team->squad.size() - 1
    );

    Player& ball_carrier = atk_team->squad[atk_pick(gen)];
    Player& marker = def_team->squad[def_pick(gen)];

    // Find goalkeeper
    Player* keeper = nullptr;

    for (auto& p : def_team->squad) {
        if (p.position == "GK") {
            keeper = &p;
            break;
        }
    }

    if (!keeper && !def_team->squad.empty()) {
        keeper = &def_team->squad[0];
    }

    double action_roll = roll(gen);

    // ======================================
    // SHOT SIMULATION
    // ======================================

    if ((ball_carrier.position == "ST" && action_roll > 0.85)
        || action_roll > 0.94) {

        atk_team->shots++;
        ball_carrier.stats.shots_total++;

        double shot_lane_p =
            sigmoid_prob(
                ball_carrier.get_eff(ball_carrier.finishing),
                marker.get_eff(marker.anticipation) + 10
            );

        if (roll(gen) < shot_lane_p) {

            ball_carrier.stats.shots_on_target++;

            double save_p =
                sigmoid_prob(
                    keeper->get_eff(keeper->reflexes) +
                    keeper->get_eff(keeper->positioning) * 0.3,

                    ball_carrier.get_eff(ball_carrier.finishing) +
                    ball_carrier.get_eff(ball_carrier.composure) * 0.2
                );

            if (roll(gen) > save_p) {

                ball_carrier.stats.goals++;
                atk_team->goals++;

                std::cout
                    << "[MIN "
                    << ms.minute
                    << "] GOAL! "
                    << ball_carrier.name
                    << " scores for "
                    << atk_team->name
                    << "!\n";

                ms.possession = def_team;
            }
            else {

                keeper->stats.saves++;
                def_team->saves++;

                ms.possession = def_team;
            }
        }
        else {
            ms.possession = def_team;
        }
    }

    // ======================================
    // DRIBBLE SIMULATION
    // ======================================

    else if (action_roll > 0.75) {

        ball_carrier.stats.dribbles_attempted++;

        double dribble_p =
            sigmoid_prob(
                ball_carrier.get_eff(ball_carrier.dribbling) +
                ball_carrier.get_eff(ball_carrier.pace) * 0.2,

                marker.get_eff(marker.tackling) +
                marker.get_eff(marker.strength) * 0.2
            );

        if (roll(gen) < dribble_p) {

            ball_carrier.stats.dribbles_won++;
        }
        else {

            double foul_p =
                marker.get_eff(marker.aggression) / 200.0;

            if (roll(gen) < foul_p) {

                marker.stats.fouls++;
                def_team->fouls++;

                std::cout
                    << "[MIN "
                    << ms.minute
                    << "] FOUL by "
                    << marker.name
                    << "!\n";

                if (roll(gen) < 0.20) {
                    marker.stats.yellow_cards++;
                    def_team->yellow++;
                }
            }
            else {

                marker.stats.tackles_won++;
                def_team->tackles++;
            }

            ms.possession = def_team;
        }
    }

    // ======================================
    // PASS SIMULATION
    // ======================================

    else {

        ball_carrier.stats.passes_attempted++;
        atk_team->pass_att++;

        double length_roll = roll(gen);

        double range_difficulty = 0.0;

        if (length_roll > 0.90)
            range_difficulty = 20.0;
        else if (length_roll > 0.65)
            range_difficulty = 5.0;

        double pass_p =
            sigmoid_prob(
                ball_carrier.get_eff(ball_carrier.passing) +
                ball_carrier.get_eff(ball_carrier.vision) * 0.3,

                marker.get_eff(marker.interceptions) +
                range_difficulty
            );

        if (roll(gen) < pass_p) {

            ball_carrier.stats.passes_completed++;
            atk_team->pass_comp++;
        }
        else {

            marker.stats.interceptions++;
            ms.possession = def_team;
        }
    }

    // Stamina reduction calculation
    ball_carrier.current_stamina =
        std::max(50.0, ball_carrier.current_stamina - 0.06);
}

// ==========================================
// REPORTING STATS
// ==========================================

void print_team_stats(const Team& t, int total_ticks) {

    double possession =
        total_ticks > 0
            ? (double)t.possession_ticks / total_ticks * 100.0
            : 0.0;

    double pass_acc =
        t.pass_att > 0
            ? (double)t.pass_comp / t.pass_att * 100.0
            : 0.0;

    std::cout
        << "\n"
        << t.name
        << " TEAM STATS\n"
        << "-----------------------------\n"
        << "Goals: " << t.goals << "\n"
        << "Possession: "
        << std::fixed
        << std::setprecision(1)
        << possession
        << "%\n"
        << "Shots: " << t.shots << "\n"
        << "Passes: "
        << t.pass_comp
        << "/"
        << t.pass_att
        << " ("
        << (int)pass_acc
        << "%)\n"
        << "Fouls: " << t.fouls << "\n"
        << "Saves: " << t.saves << "\n";
}

void print_player_stats(Team& t) {

    std::cout
        << "\nPLAYER REPORT: "
        << t.name
        << "\n";

    std::cout
        << "-------------------------------------------------------------\n";

    for (auto& p : t.squad) {

        p.update_rating();

        double pass_pc =
            p.stats.passes_attempted > 0
                ? (double)p.stats.passes_completed /
                    p.stats.passes_attempted * 100.0
                : 0.0;

        std::cout
            << std::setw(15)
            << std::left
            << p.name
            << " "
            << p.position
            << " | G:"
            << p.stats.goals
            << " | Pass:"
            << (int)pass_pc
            << "% | Rating:"
            << std::fixed
            << std::setprecision(1)
            << p.match_rating
            << "\n";
    }
}

void print_match_summary(MatchState& ms) {

    std::cout
        << "\n====================================\n"
        << "FINAL SCORE\n"
        << "====================================\n";

    std::cout
        << ms.home.name
        << " "
        << ms.home.goals
        << " - "
        << ms.away.goals
        << " "
        << ms.away.name
        << "\n";

    int total_ticks =
        ms.home.possession_ticks +
        ms.away.possession_ticks;

    print_team_stats(ms.home, total_ticks);
    print_team_stats(ms.away, total_ticks);

    print_player_stats(ms.home);
    print_player_stats(ms.away);
}

// ==========================================
// MAIN ENTRY
// ==========================================

int main() {

    try {

        int home_id;
        int away_id;

        std::cout
            << "Select Home Team ID\n"
            << "1 PSG\n"
            << "2 Arsenal\n"
            << "3 Man City\n"
            << "4 Real Madrid\n"
            << "> ";

        std::cin >> home_id;

        std::cout << "Select Away Team ID\n> ";
        std::cin >> away_id;

        MatchState ms;

        ms.home = load_team_from_csv(home_id);
        ms.away = load_team_from_csv(away_id);

        ms.possession = &ms.home;
        ms.minute = 0;

        std::mt19937 gen(12345);

        std::cout
            << "\n--- KICK OFF ---\n"
            << ms.home.name
            << " vs "
            << ms.away.name
            << "\n\n";

        for (int minute = 1; minute <= 90; minute++) {

            ms.minute = minute;

            for (int tick = 0; tick < 15; tick++) {
                resolve_event(ms, gen);
            }
        }

        print_match_summary(ms);
    }
    catch (const std::exception& e) {

        std::cerr
            << "\nFatal Simulation Error: "
            << e.what()
            << "\n";
    }

    return 0;
}
