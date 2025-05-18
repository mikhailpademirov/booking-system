#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <regex>
#include <ctime>
#include <optional>
#include <sqlite3.h>
#include <windows.h>

using namespace std;

struct Ui {
    static void separator() { cout << "\n--------------------------------------------------------\n"; }
    static void welcome(const string& name, const string& surname) { separator(); cout << "Добрый день, " << name << " " << surname << "!\n"; }
};

struct Filter { string in, out; int guests; };

struct Date { int day, month, year; };

namespace date {
    bool is_year_leap(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }
    optional<Date> parse_date(const string& date) {
        static const regex date_regex(R"(^(\d{4})\D([1-9]|0[1-9]|1[0-2])\D([1-9]|0[1-9]|[12][0-9]|3[01])$)");
        smatch matched_dates;
        if (!regex_match(date, matched_dates, date_regex)) {
            cerr << "Ошибка ввода! \n";
            return nullopt;
        }

        int year = stoi(matched_dates[1]); int month = stoi(matched_dates[2]); int day = stoi(matched_dates[3]);

        if (year < 2001 || year > 2099) {
            cerr << "Дата должна быть в диапазоне 2001 - 2099! \n";
            return nullopt;
        }

        vector <int> days_in_month = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (is_year_leap(year)) { days_in_month[1] = 29; }

        if (day > days_in_month[month - 1]) {
            cerr << "Вы вышли за пределы месяца! \n";
            return nullopt;
        }

        return Date{ day, month, year };
    }
    time_t to_time_t(const Date& date) {
        struct tm tm_date = { 0 };
        tm_date.tm_year = date.year - 1900;
        tm_date.tm_mon = date.month - 1;
        tm_date.tm_mday = date.day;
        return mktime(&tm_date);
    }
    Date get_date(int shift = 0, time_t base_time = time(nullptr)) {
        base_time += shift * 24 * 60 * 60;

        tm local_time;
        Date result;

        if (localtime_s(&local_time, &base_time) != 0) {
            cerr << "Ошибка получения времени! \n";
            return result;
        }

        result.year = local_time.tm_year + 1900;
        result.month = local_time.tm_mon + 1;
        result.day = local_time.tm_mday;
        return result;
    }
    int days_between_dates(const Date& in, const Date& out) { return difftime(to_time_t(out), to_time_t(in)) / 86400; }
    string to_str(const Date& date) {
        string year = to_string(date.year), month = to_string(date.month), day = to_string(date.day);
        if (month.size() < 2) month = '0' + month;
        if (day.size() < 2) day = '0' + day;
        return year + "-" + month + "-" + day;
    }
    Date input_date() {
        Ui::separator();
        string input;
        while (true) {
            cout << "Введите дату в формате (ГГГГ-ММ-ДД): ";
            cin >> input;
            auto new_date = parse_date(input);
            if (new_date == nullopt) continue;
            return new_date.value();
        }
    }
}

class User {
    int user_id{};
    string login, name, surname, role;
public:
    User(int id, string l, string n, string s, string r) : user_id(id), login(l), name(n), surname(s), role(r) {}
    int get_id() const { return user_id; }
    const string& get_login() const { return login; }
    const string& get_name() const { return name; }
    const string& get_surname() const { return surname; }
    const string& get_role() const { return role; }
};

class Room {
    int room_id{}, capacity{};
    string type;
    double price{};
public:
    Room(int id, string t, int c, double p) : room_id(id), type(t), capacity(c), price(p) {}
    int get_id() const { return room_id; }
    string get_type() const { return type; }
    int get_capacity() const { return capacity; }
    double get_price() const { return price; }
};

class Reservation {
    int reservation_id{}, guest_id{}, room_id{}, guests_num{};
    string in, out, status;
    double total_price{};
public:
    Reservation(int id, int g, int r, int num, string in, string out, double price, string st)
        : reservation_id(id), guest_id(g), room_id(r), guests_num(num), in(in), out(out), total_price(price), status(st) {}
    int get_reservation_id() const { return reservation_id; }
    int get_guest_id() const { return guest_id; }
    int get_room_id() const { return room_id; }
    int get_guests_num() const { return guests_num; }
    string get_in() const { return in; }
    string get_out() const { return out; }
    double get_total_price() const { return total_price; }
    string get_reservation_status() const { return status; }
};

enum ReservationStatus { NOT_STARTED, ACTIVE, OVER };

class Database {
    sqlite3* DB = nullptr;
public:
    Database(const string& path) {
        if (sqlite3_open(path.c_str(), &DB) != SQLITE_OK) {
            cerr << "Ошибка открытия базы данных: " << sqlite3_errmsg(DB) << "\n";
            exit(-1);
        }
    }
    ~Database() { if (DB) sqlite3_close(DB); }

    optional<int> get_user_id(const string& login, const string& password = "", bool check_password = false) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = nullptr;
        if (check_password) sql = "SELECT user_id FROM users WHERE login = ? AND password = ?;";
        else sql = "SELECT user_id FROM users WHERE login = ?;";

        optional<int> user_id = nullopt;

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_TRANSIENT);
            if (check_password) sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) user_id = sqlite3_column_int(stmt, 0);
        }
        else cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";

        if (stmt) sqlite3_finalize(stmt);
        return user_id;
    }
    optional<int> create_new_user(const string& login, const string& password, const vector<string>& info) {
        if (sqlite3_exec(DB, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            cerr << "Ошибка начала транзакции: " << sqlite3_errmsg(DB) << "\n";
            return nullopt;
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO users (login, password, name, surname, phone, email) VALUES (?, ?, ?, ?, ?, ?);";

        bool success = false;

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, info[0].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, info[1].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, info[2].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, info[3].c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE) cerr << "Ошибка при выполнении INSERT: " << sqlite3_errmsg(DB) << "\n";
            else success = true;
        }
        else cerr << "Ошибка при подготовке запроса: " << sqlite3_errmsg(DB) << "\n";

        if (stmt) sqlite3_finalize(stmt);

        if (success) {
            sqlite3_exec(DB, "COMMIT;", nullptr, nullptr, nullptr);
            int user_id = static_cast<int>(sqlite3_last_insert_rowid(DB));
            return user_id;
        }
        else {
            sqlite3_exec(DB, "ROLLBACK;", nullptr, nullptr, nullptr);
            return nullopt;
        }
    }
    void create_reservation(int user_id, int room_id, int guests_num, const string& in, const string& out, const string& status) {
        sqlite3_exec(DB, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        sqlite3_stmt* stmt;
        const char* sql = R"( 
        INSERT INTO bookings (user_id, room_id, guests_num, date_in, date_out, status)
        VALUES (?, ?, ?, ?, ?, ?);
    )";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_int(stmt, 2, room_id);
            sqlite3_bind_int(stmt, 3, guests_num);
            sqlite3_bind_text(stmt, 4, in.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, out.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 6, status.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                cerr << "Ошибка при выполнении INSERT: " << sqlite3_errmsg(DB) << endl;
                sqlite3_exec(DB, "ROLLBACK;", nullptr, nullptr, nullptr);
            }
            else {
                sqlite3_exec(DB, "COMMIT;", nullptr, nullptr, nullptr);
                cout << "Номер забронирован! \n";
            }

            sqlite3_finalize(stmt);
        }
        else {
            cerr << "Ошибка при подготовке запроса: " << sqlite3_errmsg(DB) << endl;
            sqlite3_exec(DB, "ROLLBACK;", nullptr, nullptr, nullptr);
        }
    }
    optional <User> get_user_by_id(int id) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT login, name, surname, role FROM users WHERE user_id = ?;";

        optional<User> user = nullopt;

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* login_c = sqlite3_column_text(stmt, 0);
                const unsigned char* name_c = sqlite3_column_text(stmt, 1);
                const unsigned char* surname_c = sqlite3_column_text(stmt, 2);
                const unsigned char* role_c = sqlite3_column_text(stmt, 3);

                string login_str = login_c ? reinterpret_cast<const char*>(login_c) : "";
                string name_str = name_c ? reinterpret_cast<const char*>(name_c) : "";
                string surname_str = surname_c ? reinterpret_cast<const char*>(surname_c) : "";
                string role_str = role_c ? reinterpret_cast<const char*>(role_c) : "";

                user.emplace(id, login_str, name_str, surname_str, role_str);
            }
            else cerr << "Пользователь с ID '" << id << "' не найден.\n";
        }
        else cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";

        if (stmt) sqlite3_finalize(stmt);
        return user;
    }
    vector<Room> new_search(const optional<Filter>& filter) {
        vector <Room> result;
        sqlite3_stmt* stmt;
        const char* sql = R"(
        SELECT r.room_id, rt.name, r.capacity, r.price
        FROM rooms AS r
        JOIN room_types AS rt ON r.type_id = rt.type_id
        WHERE NOT EXISTS (
            SELECT 1
            FROM bookings AS b
            WHERE b.room_id = r.room_id
                AND b.date_out > ?
                AND b.date_in < ?
        )
        AND r.capacity >= ?
        GROUP BY r.type_id, r.capacity;)";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, filter->in.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, filter->out.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, filter->guests);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int room_id = sqlite3_column_int(stmt, 0);
                const unsigned char* room_type = sqlite3_column_text(stmt, 1);
                int capacity = sqlite3_column_int(stmt, 2);
                double price = sqlite3_column_double(stmt, 3);
                int count = sqlite3_column_int(stmt, 4);

                string str = room_type ? reinterpret_cast<const char*>(room_type) : "";

                Room room(room_id, str, capacity, price);
                result.push_back(room);
            }
        }
        if (stmt) sqlite3_finalize(stmt);
        return result;
    }
    string get_room_type(int room_id) {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT rt.name FROM room_types AS rt JOIN rooms AS r ON rt.type_id = r.type_id WHERE room_id = ?;";
        string type = "";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, room_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* type_c = sqlite3_column_text(stmt, 0);
                type = type_c ? reinterpret_cast<const char*>(type_c) : "";
            }
        }
        else cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";
        if (stmt) sqlite3_finalize(stmt);
        return type;
    }
    optional <vector <Reservation>> get_reservations_by_status(ReservationStatus status, int user_id) {
        sqlite3_stmt* stmt = nullptr;
        vector<Reservation> user_reservations;
        string sql = R"(
        SELECT b.booking_id, b.room_id, b.user_id, b.guests_num, b.date_in, b.date_out, r.price * (JULIANDAY(b.date_out) - JULIANDAY(b.date_in)), b.status
        FROM bookings AS b JOIN rooms AS r ON b.room_id = r.room_id WHERE)";
        vector<string> conditions;

        if (user_id != 12) conditions.push_back(" b.user_id = ? ");

        if (status == NOT_STARTED) conditions.push_back(" JULIANDAY(b.date_in) > JULIANDAY('NOW') ");
        else if (status == ACTIVE) conditions.push_back(" JULIANDAY('NOW') BETWEEN JULIANDAY(b.date_in) AND JULIANDAY(b.date_out) ");
        else conditions.push_back(" JULIANDAY(b.date_out) < JULIANDAY('NOW') ");

        for (int i = 0; i < conditions.size(); i++) {
            if (i != 0) sql += " AND ";
            sql += conditions[i];
        }
        sql += " ORDER BY date_in ASC;";

        if (sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, user_id);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int booking_id = sqlite3_column_int(stmt, 0);
                int room_id = sqlite3_column_int(stmt, 1);
                int guest_id = sqlite3_column_int(stmt, 2);
                int guests_num = sqlite3_column_int(stmt, 3);
                const unsigned char* date_in = sqlite3_column_text(stmt, 4);
                const unsigned char* date_out = sqlite3_column_text(stmt, 5);
                double price = sqlite3_column_double(stmt, 6);
                const unsigned char* status = sqlite3_column_text(stmt, 7);

                string in = date_in ? reinterpret_cast<const char*>(date_in) : "";
                string out = date_out ? reinterpret_cast<const char*>(date_out) : "";
                string status_str = status ? reinterpret_cast<const char*>(status) : "";

                Reservation reservation(booking_id, guest_id, room_id, guests_num, in, out, price, status_str);
                user_reservations.push_back(reservation);
            }
        }
        else {
            cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";
            return nullopt;
        }
        if (stmt) sqlite3_finalize(stmt);
        return user_reservations;
    }
    vector<Reservation> get_reservations_by_details(const string& search_data) {
        sqlite3_stmt* stmt = nullptr;
        vector <Reservation> res_found;
        const char* sql = R"(
            SELECT
	            b.booking_id, b.user_id, b.room_id, b.guests_num, b.date_in, b.date_out,
	            r.price * (JULIANDAY(b.date_out) - JULIANDAY(b.date_in)), status
            FROM bookings AS b
            JOIN users AS u ON b.user_id = u.user_id
            JOIN rooms AS r ON b.room_id = r.room_id
            WHERE u.surname = ? OR u.phone = ? OR u.email = ?
            ORDER BY b.date_in ASC; 
        )";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, search_data.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, search_data.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, search_data.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int booking_id = sqlite3_column_int(stmt, 0);
                int user_id = sqlite3_column_int(stmt, 1);
                int room_id = sqlite3_column_int(stmt, 2);
                int guests_num = sqlite3_column_int(stmt, 3);
                const unsigned char* c_date_in = sqlite3_column_text(stmt, 4);
                const unsigned char* c_date_out = sqlite3_column_text(stmt, 5);
                double full_price = sqlite3_column_double(stmt, 6);
                const unsigned char* c_status = sqlite3_column_text(stmt, 7);

                string date_in = c_date_in ? reinterpret_cast<const char*>(c_date_in) : "";
                string date_out = c_date_out ? reinterpret_cast<const char*>(c_date_out) : "";
                string status = c_status ? reinterpret_cast<const char*>(c_status) : "";

                Reservation reservation(booking_id, user_id, room_id, guests_num, date_in, date_out, full_price, status);

                res_found.push_back(reservation);
            }
        }
        if (stmt) sqlite3_finalize(stmt);
        return res_found;
    }
    void get_payment(int id) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "UPDATE bookings SET status = 'paid' WHERE booking_id = ?;";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);

            if (sqlite3_step(stmt) == SQLITE_DONE) cout << "Оплата подтверждена! \n";
            else cerr << "Ошибка при подтверждении оплаты бронирования: " << sqlite3_errmsg(DB) << "\n";
        }
        else cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";

        if (stmt) sqlite3_finalize(stmt);
    }
    void delete_reservation(int id) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "DELETE FROM bookings WHERE booking_id = ?;";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);

            if (sqlite3_step(stmt) == SQLITE_DONE) cout << "Бронирование удалено из системы!\n";
            else cerr << "Ошибка при удалении бронирования: " << sqlite3_errmsg(DB) << "\n";
        }
        else cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";

        if (stmt) sqlite3_finalize(stmt);
    }
    void get_report_by_dates(const optional<Filter>& filter) {
        Ui::separator();
        sqlite3_stmt* stmt = nullptr;
        cout << "Отчёт: " << filter->in << " - " << filter->out << "\n";
        const char* sql = R"(
        SELECT 
	        COUNT(b.booking_id),
	        SUM(r.price) * (JULIANDAY(b.date_out) - JULIANDAY(b.date_in)),
            b.status
        FROM
	        bookings AS b
        JOIN rooms AS r ON b.room_id = r.room_id
        WHERE b.date_in >= ? AND b.date_out <= ?
        GROUP BY b.status;)";

        if (sqlite3_prepare_v2(DB, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, filter->in.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, filter->out.c_str(), -1, SQLITE_TRANSIENT);

            bool found = false;

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                found = true;
                int count = sqlite3_column_int(stmt, 0);
                double amount = sqlite3_column_double(stmt, 1);
                const unsigned char* status_c = sqlite3_column_text(stmt, 2);
                string status = status_c ? reinterpret_cast<const char*>(status_c) : "";

                cout << fixed << setprecision(2) << "Статус: " << status << " | Всего бронирований: " << count << " | Сумма: " << amount << " руб. \n";
            }
            if (!found) cout << "Нет данных за указанный период.\n";

        }
        else cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(DB) << "\n";
        if (stmt) sqlite3_finalize(stmt);
    }
};

class Validator {
    static bool check_integer(const string& str) {
        static const regex int_regex(R"(^\d+$)");
        return regex_match(str, int_regex);
    }
public:
    static int get_valid_choice(int min_value, int max_value) {
        while (true) {
            string choice;
            cin >> choice;
            if (!check_integer(choice)) {
                cout << "Ошибка ввода! Пожалуйста, введите число. \n";
                continue;
            }
            int num = stoi(choice);
            if (num < min_value || num > max_value) {
                cout << "Ошибка ввода! Число должно быть от " << min_value << " до " << max_value << ".\n";
                continue;
            }
            return num;
        }
    }
    static bool is_passwords_matches(const string& password) {
        string password_confirmation;
        cout << "Подтвердите пароль: ";
        cin >> password_confirmation;
        return password == password_confirmation;
    }
};

class UserHelper {
    static bool is_name_correct(const string& input) { return regex_match(input, regex("\\D+")); }
    static bool is_phone_correct(const string& input) { return regex_match(input, regex("\\+?[0-9]+")); }
    static bool is_email_correct(const string& input) { return regex_match(input, regex("\\w+[@]\\w+[.]\\w+")); }
    static string get_correct_input(char mode) {
        while (true) {
            string input;
            cin >> input;

            bool result = false;

            if (mode == 'n') result = is_name_correct(input);
            if (mode == 't') result = is_phone_correct(input);
            if (mode == 'e') result = is_email_correct(input);

            if (result == true) return input;

            cerr << "Ошибка ввода! \n";
        }
    }
public:
    static vector <string> input_user_information() {
        cout << "Имя: ";
        string name = get_correct_input('n');
        cout << "Фамилия: ";
        string surname = get_correct_input('n');
        cout << "Телефон: ";
        string phone = get_correct_input('t');
        cout << "Email: ";
        string email = get_correct_input('e');
        return { name, surname, phone, email };
    }
    static int input_guests() {
        Ui::separator();
        cout << "Введите количество гостей: ";
        return Validator::get_valid_choice(0, 9);
    }
};

class AuthManager {
    Database& db;
    int user_id = 0;
public:
    AuthManager(Database& database) : db(database) {}
    int authorization() {
        while (true) {
            Ui::separator();
            cout << "Пожалуйста, введите ваш логин: ";
            string login, password;
            cin >> login;

            if (login == "0") {
                Ui::separator();
                return 0;
            }

            cout << "Введите пароль: ";
            cin >> password;

            auto id = db.get_user_id(login, password, true);

            if (id == nullopt) {
                cerr << "Ошибка входа! Проверьте правильность логина и пароля. \n";
                continue;
            }

            cout << "Авторизация выполнена успешно! \n";
            return user_id = id.value();
        }
    }
    int registration() {
        while (true) {
            Ui::separator();
            cout << "Придумайте логин: ";
            string login;
            cin >> login;

            if (login == "0") {
                Ui::separator();
                return 0;
            }

            if (db.get_user_id(login) != nullopt) {
                cout << "Логин занят!\n";
                continue;
            }

            cout << "Логин " << login << " свободен! \n";
            string password;
            while (true) {
                cout << "Придумайте пароль: ";
                cin >> password;
                if (Validator::is_passwords_matches(password)) break;
                cout << "Пароли не совпадают! \n";
            }

            vector<string> user_information = UserHelper::input_user_information();

            auto id = db.create_new_user(login, password, user_information);
            if (id == nullopt) {
                cerr << "Ошибка регистрации! \n";
                continue;
            }

            cout << "Вы успешно зарегестрировались! \n";
            return user_id = id.value();
        }
    }
    int get_user_id() const { return user_id; };
};

class BookingSystem {
protected:
    Database& db;
    optional <User> user;
    optional <Filter> build_filter() {
        Date date_in = date::get_date(), date_out = date::get_date(1);
        int guests = 2;

        while (true) {
            Ui::separator();
            cout << "Фильтр: " << date::to_str(date_in) << " - " << date::to_str(date_out) << " | Гостей: " << guests << "\n1. Заезд \n2. Выезд \n3. Гости \n4. Поиск \n0. Вернуться в меню \n";

            int choice = Validator::get_valid_choice(0, 4);

            switch (choice) {
            case 0: return nullopt;
            case 1: {
                Date temp_in = date::input_date();
                if (date::to_time_t(temp_in) >= date::to_time_t(date_out)) date_out = date::get_date(1, date::to_time_t(temp_in));
                if (date::to_time_t(temp_in) <= date::to_time_t(date::get_date())) {
                    cerr << "Дата заезда должна быть позже сегодняшнего дня! \n";
                    break;
                }
                date_in = temp_in;
                break;
            }
            case 2: {
                Date temp_out = date::input_date();
                if (date::to_time_t(temp_out) <= date::to_time_t(date_in)) cerr << "Дата выезда должна быть позже даты заезда! \n";
                else date_out = temp_out;
                break;
            }
            case 3: {
                int temp_guests = UserHelper::input_guests();
                if (temp_guests > 0) guests = temp_guests;
                break;
            }
            case 4: return Filter{ date::to_str(date_in), date::to_str(date_out), guests };
            }
        }
    }
    int room_choice(const vector<Room>& rooms_found) {
        while (true) {
            Ui::separator();
            cout << "Пожалуйста, выберите комнату (0 - назад): \n";
            int counter = 1;
            for (int i = 0; i < rooms_found.size(); i++, counter++) {
                string type = rooms_found[i].get_type();
                int capacity = rooms_found[i].get_capacity();
                double price = rooms_found[i].get_price();
                cout << counter << ". " << type << " | Вместимость: " << capacity << " чел. | Цена за ночь: " << price << " руб. \n";
            }

            if (counter == 0) {
                cout << "По вашим параметрам не найдено свободных комнат! \n";
                return 0;
            }

            int choice = Validator::get_valid_choice(0, rooms_found.size());
            if (choice == 0) return 0;
            return choice;
        }
    }
    bool is_reservation_details(int days, double full_price, const Room& room, const optional<Filter>& filter) {
        Ui::separator();
        cout << "Подтвердите данные (1 - Да / 0 - Нет): \nДаты: " << filter->in << " - " << filter->out << "\nДней: " << days
            << "\nГости: " << filter->guests << "\nКатегория номера: " << room.get_type() << "\nИтого: " << full_price << " руб. \n";
        int choice = Validator::get_valid_choice(0, 1);
        return choice == 1;
    }
    optional<string> choose_payment_method(double full_price) {
        Ui::separator();
        cout << "К оплате: " << full_price << " руб. \nСпособ оплаты: \n1. Картой онлайн \n2. При заселении \n0. Назад \n";

        int choice = Validator::get_valid_choice(0, 2);
        switch (choice) {
        case 0: return nullopt;
        case 1: return "paid";
        case 2: return "not paid";
        }
    }
    void reservation_process(const optional<User>& user) {
        while (true) {
            auto filter = build_filter();

            if (filter == nullopt) return;

            auto available_rooms = db.new_search(filter);
            if (available_rooms.empty()) {
                cout << "Нет доступных номеров по заданным параметрам! \n";
                continue;
            }

            while (true) {
                int room_num = room_choice(available_rooms);
                if (room_num == 0) break;

                int days = date::days_between_dates(date::parse_date(filter->in).value(), date::parse_date(filter->out).value());
                double full_price = available_rooms[room_num - 1].get_price() * days;

                if (!is_reservation_details(days, full_price, available_rooms[room_num - 1], filter)) continue;

                auto payment_type = choose_payment_method(full_price);
                if (payment_type == nullopt) continue;

                int user_id = 0;
                if (user != nullopt) user_id = user->get_id();
                else {
                    vector<string> user_info = UserHelper::input_user_information();
                    string new_login = user_info[2], new_password = user_info[2];
                    while (true) {
                        if (db.get_user_id(new_login) == nullopt) break;
                        new_login += to_string(new_login.size());
                    }
                    user_id = db.create_new_user(new_login, new_password, user_info).value();
                }

                db.create_reservation(user_id, available_rooms[room_num - 1].get_id(), filter->guests, filter->in, filter->out, payment_type.value());
                return;
            }
        }
    }
    virtual void print_bookings(const optional<vector<Reservation>>& reservations_list) {
        Ui::separator();
        int count = 0, n = 1;
        for (int i = 0; i < reservations_list->size(); i++, n++, count++) {
            const Reservation& res = (*reservations_list)[i];
            cout << n << "." << "Категория номера: " << db.get_room_type(res.get_room_id()) << "\n"
                << "Гости: " << res.get_guests_num() << "\nДаты: " << res.get_in() << " - " << res.get_out() << "\nСтоимость: " << res.get_total_price() << " руб.\n\n";
        }

        if (count == 0) cout << "Бронирования не найдены! \n";
    }
    void reservations() {
        while (true) {
            Ui::separator();
            cout << "Выберите действие: \n1. Активные \n2. Завершенные \n3. Предстоящие \n0. Назад \n";
            int choice = Validator::get_valid_choice(0, 3);

            switch (choice) {
            case 0: return;
            case 1:
                print_bookings(db.get_reservations_by_status(ACTIVE, user->get_id()));
                break;
            case 2:
                print_bookings(db.get_reservations_by_status(OVER, user->get_id()));
                break;
            case 3:
                print_bookings(db.get_reservations_by_status(NOT_STARTED, user->get_id()));
                break;
            }
        }
    }
private:
    void guest_process() {
        while (true) {
            Ui::welcome(user->get_name(), user->get_surname());
            cout << "1. Новое бронирование \n2. Мои бронирования \n0. Выйти из профиля \n";
            int choice = Validator::get_valid_choice(0, 2);

            switch (choice) {
            case 0: return;
            case 1:
                reservation_process(user);
                break;
            case 2:
                reservations();
                break;
            }
        }
    }
public:
    BookingSystem(Database& database, User _user) : db(database) {
        user = _user;
    }
    virtual void start() { guest_process(); }
};

class AdminSystem : public BookingSystem {
    void print_bookings(const optional<vector<Reservation>>& reservations_list) {
        Ui::separator();
        int count = 0, n = 1;
        for (int i = 0; i < reservations_list->size(); i++, n++, count++) {
            const Reservation& res = (*reservations_list)[i];
            auto guest = db.get_user_by_id(res.get_guest_id());
            cout << n << "." << "" "Категория номера: " << db.get_room_type(res.get_room_id()) << "\n"
                << "Имя: " << guest->get_name() << " " << guest->get_surname() << "\n"
                << "Гости: " << res.get_guests_num() << " | Номер комнаты: " << res.get_room_id() << " | Даты: " << res.get_in() << " - " << res.get_out() << "\n"
                << "Стоимость: " << res.get_total_price() << " | Статус: " << res.get_reservation_status() << "\n\n";
        }

        if (count == 0) cout << "Бронирования не найдены! \n";
    }
    int find_reservation_id(const vector<Reservation>& r_found) {
        while (true) {
            Ui::separator();
            int counter = 1;
            for (int i = 0; i < r_found.size(); i++, counter++) {
                const auto& res = r_found[i];
                optional<User> user = db.get_user_by_id(res.get_guest_id());

                cout << counter << ". " << "" "Категория номера: " << db.get_room_type(res.get_room_id()) << "\n"
                    << "Имя: " << user->get_name() << " " << user->get_surname() << "\n"
                    << "Гости: " << res.get_guests_num() << " | Номер комнаты: " << res.get_room_id() << " | Даты: " << res.get_in() << " - " << res.get_out() << "\n"
                    << "Стоимость: " << res.get_total_price() << " | Статус: " << res.get_reservation_status() << "\n\n";
            }

            cout << "Выберите бронирование (0 — назад): ";
            int choice = Validator::get_valid_choice(0, r_found.size());
            if (choice == 0) return 0;

            return r_found[choice - 1].get_reservation_id();
        }
    }
    void change_reservation(int id) {
        Ui::separator();
        cout << "\n1. Принять оплату \n2. Отменить бронирование \n0. Назад \n";

        int choice = Validator::get_valid_choice(0, 2);
        switch (choice) {
        case 0: return;
        case 1:
            db.get_payment(id);
            break;
        case 2:
            db.delete_reservation(id);
            return;
        }
    }
    void manage_bookings() {
        while (true) {
            Ui::separator();
            cout << "Искать по фамилии, телефону или email (0 — назад): ";
            string search_data;
            cin >> search_data;
            if (search_data == "0") return;

            auto reservations_result = db.get_reservations_by_details(search_data);
            if (reservations_result.empty()) {
                cout << "Бронирования не найдены! \n";
                continue;
            }

            int chosen_num = find_reservation_id(reservations_result);
            if (chosen_num == 0) continue;

            change_reservation(chosen_num);
        }
    }
    optional<Filter> build_report() {
        Date date_in = date::get_date(), date_out = date::get_date(1);

        while (true) {
            Ui::separator();
            cout << "Отчет: " << date::to_str(date_in) << " - " << date::to_str(date_out) << "\n"
                << "1. Начало \n2. Конец \n3. Составить отчёт \n0. Вернуться в меню \n";

            int choice = Validator::get_valid_choice(0, 3);
            switch (choice) {
            case 0: return nullopt;
            case 1: {
                Date temp_in = date::input_date();
                if (date::to_time_t(temp_in) >= date::to_time_t(date_out)) date_out = date::get_date(1, date::to_time_t(temp_in));
                date_in = temp_in;
                break;
            }
            case 2: {
                Date temp_out = date::input_date();
                if (date::to_time_t(temp_out) <= date::to_time_t(date_in)) cerr << "Дата начала должна быть раньше даты конца \n";
                else date_out = temp_out;
                break;
            }
            case 3: return Filter{ date::to_str(date_in), date::to_str(date_out) };
            }
        }
    }
    void admin_process() {
        while (true) {
            Ui::welcome(user->get_name(), user->get_surname());
            cout << "1. Зарегестрировать гостя \n2. Управлять бронированиями \n3. Отчёт по датам \n4. Обзор бронирований \n0. Выйти из профиля \n";
            int choice = Validator::get_valid_choice(0, 4);

            switch (choice) {
            case 0: return;
            case 1:
                reservation_process(nullopt);
                break;
            case 2:
                manage_bookings();
                break;
            case 3: {
                auto report_filter = build_report();
                if (report_filter) db.get_report_by_dates(report_filter);
                break;
            }
            case 4:
                reservations();
                break;
            }
        }
    }
public:
    using BookingSystem::BookingSystem;
    void start() { admin_process(); }
};

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    Database db("db/base.db");
    
    while (true) {
        AuthManager auth(db);
        cout << "=== СИСТЕМА УПРАВЛЕНИЯ БРОНИРОВАНИЯ МЕСТ В ГОСТИНИЦЕ === \n1. Авторизация \n2. Регистрация \n0. Выйти из системы \n";

        int choice = Validator::get_valid_choice(0, 2);
        switch (choice) {
        case 0: return 0;
        case 1:
            auth.authorization();
            break;
        case 2:
            auth.registration();
            break;
        }
        if (!auth.get_user_id()) continue;

        if (auto user = db.get_user_by_id(auth.get_user_id())) {
            if (user->get_role() == "admin") {
                AdminSystem system(db, user.value());
                system.start();
            }
            else {
                BookingSystem system(db, user.value());
                system.start();
            }
        }
    }
    return 0;
}
