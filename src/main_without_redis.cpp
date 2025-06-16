// src/main.cpp

#include <crow.h>
#include <pqxx/pqxx>
#include <iostream>
#include <vector>
#include <utility> // для std::move

int main() {
    crow::SimpleApp app;

    // Строка подключения к БД.
    const std::string conn_str = "dbname=blogdb user=bloguser";

    // Эндпоинт: получить все статьи с комментариями
    CROW_ROUTE(app, "/articles")([&conn_str]() {
        crow::json::wvalue result;
        // Вектор для статей
        std::vector<crow::json::wvalue> articles_list;

        try {
            pqxx::connection conn(conn_str);
            if (!conn.is_open()) {
                return crow::response(500, "Failed to open DB connection");
            }
            // Подготовка запроса для комментариев
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

                // Получаем комментарии и собираем в вектор
                pqxx::result comments = tx.exec_prepared("get_comments", id);
                std::vector<crow::json::wvalue> comments_list;
                for (const auto &c : comments) {
                    crow::json::wvalue cm;
                    cm["id"] = c[0].as<int>();
                    cm["content"] = c[1].c_str();
                    comments_list.push_back(cm);
                }
                // Перемещаем вектор в wvalue
                article_json["comments"] = std::move(comments_list);

                articles_list.push_back(article_json);
            }

            tx.commit();
        }
        catch (const std::exception &e) {
            std::cerr << "Exception in /articles handler: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        // Присваиваем результат перемещением
        result["articles"] = std::move(articles_list);
        return crow::response(result);
    });

    // Эндпоинт: получить одну статью по id
    CROW_ROUTE(app, "/article/<int>")([&conn_str](int article_id) {
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
        return crow::response(result);
    });

    app.port(18080).multithreaded().run();
    return 0;
}
