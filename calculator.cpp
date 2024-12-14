#include<crow.h>
using namespace std;

int calculate(int a, int b, string op) {
	if( op == "+")
		return a+b;
	if( op == "-")
		return a-b;
	if( op == "*")
		return a*b;
	if( op == "/")
		return a/b;
	return -1;
}

int main()
{
	crow::SimpleApp app;
	CROW_ROUTE(app, "/calculator").methods("POST"_method)([](const crow::request &req) {
		try {
			auto body = crow::json::load(req.body);
			if (!body || !body["a"] || !body["b"] || !body["op"]) {
				return crow::response(400, "Invalid Request");
			}
			int a = body["a"].i();
			int b = body["b"].i();
			string op = body["op"].s();
			int c = calculate(a,b,op);
			crow::json::wvalue res;
			res["Result"] = c;
			return crow::response(res);
		} catch (exception &e) {
			return crow::response(400, e.what());
		}
	});
	app.bindaddr("127.0.0.1").port(18080).multithreaded().run();
}
