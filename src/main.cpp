// src/main.cpp

#include <crow.h>
#include <pqxx/pqxx>
#include <sw/redis++/redis++.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>

using namespace sw::redis;

int main() {
    crow::SimpleApp app;

    // Строка подключения к PostgreSQL. Скорректируйте под своё окружение:
    // например: "dbname=blogdb user=bloguser host=127.0.0.1 port=5432 password=..."
    const std::string conn_str = "dbname=blogdb user=bloguser";

    // Инициализация клиента Valkey/Redis: используем уникальный указатель
    std::unique_ptr<Redis> redis_client;
    try {
        // При необходимости укажите URI с паролем: "tcp://:password@127.0.0.1:6379/0"
        redis_client = std::make_unique<Redis>("tcp://127.0.0.1:6379");
    }
    catch (const std::exception &e) {
        std::cerr << "Ошибка подключения к Valkey: " << e.what() << std::endl;
        // redis_client останется nullptr, последующие вызовы через redis_client будут пропускаться
    }

    // GET /articles
    CROW_ROUTE(app, "/articles")([&conn_str, &redis_client]() {
        const std::string cache_key = "articles_all";

        // 1) Попытка взять из кеша
        if (redis_client) {
            try {
                auto cached = redis_client->get(cache_key);
                if (cached) {
                    std::string json_str = *cached;
                    crow::response res(json_str);
                    res.set_header("Content-Type", "application/json");
                    return res;
                }
            }
            catch (const std::exception &e) {
                std::cerr << "Valkey GET error (articles_all): " << e.what() << std::endl;
                // продолжаем к БД
            }
        }

        // 2) Кеша нет или redis недоступен: запрос к БД
        crow::json::wvalue result;
        std::vector<crow::json::wvalue> articles_list;
        try {
            pqxx::connection conn(conn_str);
            if (!conn.is_open()) {
                return crow::response(500, "Failed to open DB connection");
            }
            // Подготовка запроса комментариев
            try {
                conn.prepare("get_comments", "SELECT id, content FROM comments WHERE article_id = $1");
            }
            catch (...) {}
            pqxx::work tx(conn);

            pqxx::result articles = tx.exec("SELECT id, title, content FROM articles");
            for (const auto &row : articles) {
                crow::json::wvalue article_json;
                int id = row[0].as<int>();
                article_json["id"] = id;
                article_json["title"] = row[1].c_str();
                article_json["content"] = row[2].c_str();

                // Получаем комментарии
                pqxx::result comments = tx.exec_prepared("get_comments", id);
                std::vector<crow::json::wvalue> comments_list;
                for (const auto &c : comments) {
                    crow::json::wvalue cm;
                    cm["id"] = c[0].as<int>();
                    cm["content"] = c[1].c_str();
                    comments_list.push_back(cm);
                }
                article_json["comments"] = std::move(comments_list);

                articles_list.push_back(article_json);
            }
            tx.commit();
        }
        catch (const std::exception &e) {
            std::cerr << "Exception in /articles handler: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        result["articles"] = std::move(articles_list);

        // 3) Сериализация JSON
        std::string json_str;
        try {
            json_str = result.dump();
        }
        catch (const std::exception &e) {
            std::cerr << "JSON dump error: " << e.what() << std::endl;
            return crow::response(500, "JSON serialization error");
        }

        // 4) Сохранение в кеш
        if (redis_client) {
            try {
                redis_client->set(cache_key, json_str);
                redis_client->expire(cache_key, 60);
            }
            catch (const std::exception &e) {
                std::cerr << "Valkey SET error (articles_all): " << e.what() << std::endl;
            }
        }

        crow::response res(json_str);
        res.set_header("Content-Type", "application/json");
        return res;
    });

    // GET /article/<id>
    CROW_ROUTE(app, "/article/<int>")([&conn_str, &redis_client](int article_id) {
        const std::string cache_key = "article:" + std::to_string(article_id);

        // 1) Попытка взять из кеша
        if (redis_client) {
            try {
                auto cached = redis_client->get(cache_key);
                if (cached) {
                    std::string json_str = *cached;
                    crow::response res(json_str);
                    res.set_header("Content-Type", "application/json");
                    return res;
                }
            }
            catch (const std::exception &e) {
                std::cerr << "Valkey GET error (" << cache_key << "): " << e.what() << std::endl;
            }
        }

        // 2) Запрос к БД
        crow::json::wvalue result;
        try {
            pqxx::connection conn(conn_str);
            if (!conn.is_open()) {
                return crow::response(500, "DB connection failed");
            }
            try {
                conn.prepare("get_article", "SELECT id, title, content FROM articles WHERE id = $1");
                conn.prepare("get_comments", "SELECT id, content FROM comments WHERE article_id = $1");
            }
            catch (...) {}
            pqxx::work tx(conn);

            pqxx::result art = tx.exec_prepared("get_article", article_id);
            if (art.empty()) {
                return crow::response(404, "Article not found");
            }
            const auto &row = art[0];
            result["id"] = row[0].as<int>();
            result["title"] = row[1].c_str();
            result["content"] = row[2].c_str();

            pqxx::result comments = tx.exec_prepared("get_comments", article_id);
            std::vector<crow::json::wvalue> comments_list;
            for (const auto &c : comments) {
                crow::json::wvalue cm;
                cm["id"] = c[0].as<int>();
                cm["content"] = c[1].c_str();
                comments_list.push_back(cm);
            }
            result["comments"] = std::move(comments_list);

            tx.commit();
        }
        catch (const std::exception &e) {
            std::cerr << "Exception in /article handler: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        // 3) Сериализация JSON
        std::string json_str;
        try {
            json_str = result.dump();
        }
        catch (const std::exception &e) {
            std::cerr << "JSON dump error: " << e.what() << std::endl;
            return crow::response(500, "JSON serialization error");
        }

        // 4) Сохранение в кеш
        if (redis_client) {
            try {
                redis_client->set(cache_key, json_str);
                redis_client->expire(cache_key, 60);
            }
            catch (const std::exception &e) {
                std::cerr << "Valkey SET error (" << cache_key << "): " << e.what() << std::endl;
            }
        }

        crow::response res(json_str);
        res.set_header("Content-Type", "application/json");
        return res;
    });

    // GET /article/random с кэшированием конкретной статьи
    CROW_ROUTE(app, "/article/random")([&conn_str, &redis_client]() {
        // 1) Открываем соединение к БД, получаем случайный id
        int article_id = -1;
        try {
            pqxx::connection conn(conn_str);
            if (!conn.is_open()) {
                return crow::response(500, "DB connection failed");
            }
            // Подготовка запроса на случайный id
            try {
                // Если уже подготовлено ранее, catch проигнорит повтор
                conn.prepare("get_random_id", "SELECT id FROM articles ORDER BY RANDOM() LIMIT 1");
                conn.prepare("get_comments", "SELECT id, content FROM comments WHERE article_id = $1");
                conn.prepare("get_article", "SELECT id, title, content FROM articles WHERE id = $1");
            }
            catch (...) {}
            pqxx::work tx(conn);

            pqxx::result rnd = tx.exec_prepared("get_random_id");
            if (rnd.empty()) {
                // Нет статей
                return crow::response(404, "No articles available");
            }
            article_id = rnd[0][0].as<int>();
            tx.commit();
        }
        catch (const std::exception &e) {
            std::cerr << "Exception selecting random id: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        // 2) Сформируем ключ кеша для этой статьи
        const std::string cache_key = "article:" + std::to_string(article_id);

        // 3) Попытка взять из кеша
        if (redis_client) {
            try {
                if (auto cached = redis_client->get(cache_key)) {
                    // Если в кеше найдена статья, возвращаем сразу
                    std::string json_str = *cached;
                    crow::response res(json_str);
                    res.set_header("Content-Type", "application/json");
                    return res;
                }
            }
            catch (const std::exception &e) {
                std::cerr << "Redis GET error (" << cache_key << "): " << e.what() << std::endl;
                // продолжаем к БД
            }
        }

        // 4) Если не в кеше, запрашиваем из БД статью и комментарии
        crow::json::wvalue result;
        try {
            pqxx::connection conn(conn_str);
            if (!conn.is_open()) {
                return crow::response(500, "DB connection failed");
            }
            // Подготовка запросов (если не подготовлены ранее)
            try {
                conn.prepare("get_article", "SELECT id, title, content FROM articles WHERE id = $1");
                conn.prepare("get_comments", "SELECT id, content FROM comments WHERE article_id = $1");
            }
            catch (...) {}
            pqxx::work tx(conn);

            pqxx::result art = tx.exec_prepared("get_article", article_id);
            if (art.empty()) {
                // На всякий случай, если статья исчезла
                return crow::response(404, "Article not found");
            }
            const auto &row = art[0];
            result["id"] = row[0].as<int>();
            result["title"] = row[1].c_str();
            result["content"] = row[2].c_str();

            pqxx::result comments = tx.exec_prepared("get_comments", article_id);
            std::vector<crow::json::wvalue> comments_list;
            for (const auto &c : comments) {
                crow::json::wvalue cm;
                cm["id"] = c[0].as<int>();
                cm["content"] = c[1].c_str();
                comments_list.push_back(cm);
            }
            result["comments"] = std::move(comments_list);

            tx.commit();
        }
        catch (const std::exception &e) {
            std::cerr << "Exception in /article/random DB fetch: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        // 5) Сериализуем JSON
        std::string json_str;
        try {
            json_str = result.dump();
        }
        catch (const std::exception &e) {
            std::cerr << "JSON dump error in /article/random: " << e.what() << std::endl;
            return crow::response(500, "JSON serialization error");
        }

        // 6) Сохраняем в кеш
        if (redis_client) {
            try {
                redis_client->set(cache_key, json_str);
                // Можно выставить TTL, например 60 секунд:
                redis_client->expire(cache_key, 60);
            }
            catch (const std::exception &e) {
                std::cerr << "Redis SET error (" << cache_key << "): " << e.what() << std::endl;
                // игнорируем ошибку кеша
            }
        }

        // 7) Возвращаем ответ
        crow::response res(json_str);
        res.set_header("Content-Type", "application/json");
        return res;
    });


    // Запуск сервера на порту 18080
    app.port(18080).multithreaded().run();
    return 0;
}
