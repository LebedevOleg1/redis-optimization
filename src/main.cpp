#include <crow.h>
#include <pqxx/pqxx>
#include <sw/redis++/redis++.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>

using namespace sw::redis;

int main() {
    crow::SimpleApp app;

    const std::string conn_str = "dbname=blogdb user=bloguser";
    std::unique_ptr<Redis> redis_client;
    try {
        redis_client = std::make_unique<Redis>("tcp://127.0.0.1:6379");
    }
    catch (const std::exception &e) {
        std::cerr << "Ошибка подключения к Valkey: " << e.what() << std::endl;
    }

    CROW_ROUTE(app, "/article/random")([&conn_str, &redis_client]() {
        auto start = std::chrono::high_resolution_clock::now();

        int article_id = -1;
        try {
            pqxx::connection conn(conn_str);
            if (!conn.is_open()) {
                return crow::response(500, "DB connection failed");
            }
            try {
                conn.prepare("get_random_id", "SELECT id FROM articles ORDER BY RANDOM() LIMIT 1");
                conn.prepare("get_comments", "SELECT id, content FROM comments WHERE article_id = $1");
                conn.prepare("get_article", "SELECT id, title, content FROM articles WHERE id = $1");
            }
            catch (...) {}
            pqxx::work tx(conn);

            pqxx::result rnd = tx.exec_prepared("get_random_id");
            if (rnd.empty()) {
                return crow::response(404, "No articles available");
            }
            article_id = rnd[0][0].as<int>();
            tx.commit();
        }
        catch (const std::exception &e) {
            std::cerr << "Exception selecting random id: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        const std::string cache_key = "article:" + std::to_string(article_id);

        if (redis_client) {
            try {
                if (auto cached = redis_client->get(cache_key)) {
                    std::string json_str = *cached;
                    crow::response res(json_str);
                    res.set_header("Content-Type", "application/json");

                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    std::cout << "Request (cached) took " << duration << "ms" << std::endl;
                    std::ofstream ofs("response_times.txt", std::ios::app);
                    ofs << duration << "\n";
                    return res;
                }
            }
            catch (const std::exception &e) {
                std::cerr << "Redis GET error (" << cache_key << "): " << e.what() << std::endl;
            }
        }

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
            std::cerr << "Exception in /article/random DB fetch: " << e.what() << std::endl;
            return crow::response(500, std::string("Exception: ") + e.what());
        }

        std::string json_str;
        try {
            json_str = result.dump();
        }
        catch (const std::exception &e) {
            std::cerr << "JSON dump error in /article/random: " << e.what() << std::endl;
            return crow::response(500, "JSON serialization error");
        }

        if (redis_client) {
            try {
                redis_client->set(cache_key, json_str);
                redis_client->expire(cache_key, 110);
            }
            catch (const std::exception &e) {
                std::cerr << "Redis SET error (" << cache_key << "): " << e.what() << std::endl;
            }
        }

        crow::response res(json_str);
        res.set_header("Content-Type", "application/json");

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "Request (DB) took " << duration << "ms" << std::endl;
        std::ofstream ofs("response_times.txt", std::ios::app);
        ofs << duration << "\n";
        return res;
    });

    app.port(18080).multithreaded().run();
    return 0;
}