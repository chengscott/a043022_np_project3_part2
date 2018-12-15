#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
using namespace boost::asio::ip;

boost::asio::io_service ioservice;

class Session : public std::enable_shared_from_this<Session> {
public:
	Session(tcp::socket socket) : socket_(std::move(socket)) {}

	void start() { parse_request(); }

private:
	void close() { socket_.close(); }

	void parse_request() {
		auto self(shared_from_this());
		boost::asio::async_read_until(
			socket_, buffer_, "\r\n",
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				std::string req((std::istreambuf_iterator<char>(&buffer_)),
					std::istreambuf_iterator<char>());
				std::cout << req << std::endl;
				std::string method, url, protocol;
				std::stringstream ss(req);
				ss >> method >> url >> protocol;
				if (method != "GET")
					throw std::logic_error("Request is not GET");
				const size_t pos = url.find("?");
				req_file_ = url.substr(1, pos - 1);
				req_query_ = url.substr(pos + 1, url.length() - pos);
				if (req_file_ == "console.cgi") {
					exec_console_cgi(protocol);
				}
				else if (req_file_ == "panel.cgi") {
					exec_panel_cgi(protocol);
				}
				else if (req_file_ == "welcome.cgi") {
					exec_welcome_cgi(protocol);
				}
				else {
					http_not_found(protocol);
				}
			}
		});
	}

	void http_not_found(const std::string &protocol) {
		buf_ = protocol + " 404 Not Found\r\n";
		auto self(shared_from_this());
		boost::asio::async_write(socket_, boost::asio::buffer(buf_),
			[this, self](boost::system::error_code ec,
				std::size_t length) { close(); });
	}

	void exec_console_cgi(const std::string &protocol) {
		buf_ = protocol + " 200 OK\r\n";
		buf_ += "Content-type: text/html\r\n\r\n";
		// parse_query
		for (size_t i = 0; i < 5; ++i)
			query_[i].session = "s" + std::to_string(i);
		std::regex pattern("&?([^=]+)=([^&]+)");
		auto it =
			std::sregex_iterator(req_query_.begin(), req_query_.end(), pattern);
		auto it_end = std::sregex_iterator();
		for (; it != it_end; ++it) {
			std::string key = (*it)[1].str(), value = (*it)[2].str();
			int idx = key[1] - '0';
			if (key[0] == 'h')
				query_[idx].host = value;
			else if (key[0] == 'p')
				query_[idx].port = value;
			else if (key[0] == 'f')
				query_[idx].file = value;
		}
		// init_console
		buf_ += R"(<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>NP Project 3 Console</title>
    <link
      rel="stylesheet"
      href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
      integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
      crossorigin="anonymous"
    />
    <link
      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
      rel="stylesheet"
    />
    <link
      rel="icon"
      type="image/png"
      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
    />
    <style>
      * {
        font-family: 'Source Code Pro', monospace;
        font-size: 1rem !important;
      }
      body {
        background-color: #212529;
      }
      pre {
        color: #cccccc;
      }
      b {
        color: #ffffff;
      }
    </style>
  </head>
  <body>
    <table class="table table-dark table-bordered">
		)";
		buf_ += "<thead><tr>";
		for (const Query &q : query_)
			if (!q.host.empty())
				buf_ += R"(<th scope="col">)" + q.host + ':' + q.port + "</th>";
		buf_ += "</tr></thead><tbody><tr>";
		for (const Query &q : query_)
			if (!q.host.empty())
				buf_ +=
				R"(<td><pre id=")" + q.session + R"(" class="mb-0"></pre></td>)";
		buf_ += "</tr></tbody>";
		buf_ += R"~~~(
    </table>
  </body>
</html>
		)~~~";
		buf_ += "\r\n";
		auto self(shared_from_this());
		boost::asio::async_write(
			socket_, boost::asio::buffer(buf_),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				for (const Query &q : query_)
					if (!q.host.empty())
						std::make_shared<Client>(self, q.host, q.port, q.file,
							q.session)->connect();
			}
		});
	}

	void exec_panel_cgi(const std::string &protocol) {
		buf_ = protocol + " 200 OK\r\n";
		buf_ += "Content-type: text/html\r\n\r\n";
		buf_ += R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <title>NP Project 3 Panel</title>
    <link rel="stylesheet"
          href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
          integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
          crossorigin="anonymous" />
    <link href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
          rel="stylesheet" />
    <link rel="icon"
          type="image/png"
          href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png" />
    <style>
        * {
            font-family: 'Source Code Pro', monospace;
        }
    </style>
</head>
<body class="bg-secondary pt-5">
    <form action="console.cgi" method="GET">
        <table class="table mx-auto bg-light" style="width: inherit">
            <thead class="thead-dark">
                <tr>
                    <th scope="col">#</th>
                    <th scope="col">Host</th>
                    <th scope="col">Port</th>
                    <th scope="col">Input File</th>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <th scope="row" class="align-middle">Session 1</th>
                    <td>
                        <div class="input-group">
                            <select name="h0" class="custom-select">
                                <option></option>
                                <option value="nplinux1.cs.nctu.edu.tw">nplinux1</option>
                                <option value="nplinux2.cs.nctu.edu.tw">nplinux2</option>
                                <option value="nplinux3.cs.nctu.edu.tw">nplinux3</option>
                                <option value="nplinux4.cs.nctu.edu.tw">nplinux4</option>
                                <option value="nplinux5.cs.nctu.edu.tw">nplinux5</option>
                                <option value="npbsd1.cs.nctu.edu.tw">npbsd1</option>
                                <option value="npbsd2.cs.nctu.edu.tw">npbsd2</option>
                                <option value="npbsd3.cs.nctu.edu.tw">npbsd3</option>
                                <option value="npbsd4.cs.nctu.edu.tw">npbsd4</option>
                                <option value="npbsd5.cs.nctu.edu.tw">npbsd5</option>
                            </select>
                            <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                            </div>
                        </div>
                    </td>
                    <td>
                        <input name="p0" type="text" class="form-control" size="5" />
                    </td>
                    <td>
                        <select name="f0" class="custom-select">
                            <option></option>
                            <option value="t1.txt">t1.txt</option>
                            <option value="t2.txt">t2.txt</option>
                            <option value="t3.txt">t3.txt</option>
                            <option value="t4.txt">t4.txt</option>
                            <option value="t5.txt">t5.txt</option>
                            <option value="t6.txt">t6.txt</option>
                            <option value="t7.txt">t7.txt</option>
                            <option value="t8.txt">t8.txt</option>
                            <option value="t9.txt">t9.txt</option>
                            <option value="t10.txt">t10.txt</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <th scope="row" class="align-middle">Session 2</th>
                    <td>
                        <div class="input-group">
                            <select name="h1" class="custom-select">
                                <option></option>
                                <option value="nplinux1.cs.nctu.edu.tw">nplinux1</option>
                                <option value="nplinux2.cs.nctu.edu.tw">nplinux2</option>
                                <option value="nplinux3.cs.nctu.edu.tw">nplinux3</option>
                                <option value="nplinux4.cs.nctu.edu.tw">nplinux4</option>
                                <option value="nplinux5.cs.nctu.edu.tw">nplinux5</option>
                                <option value="npbsd1.cs.nctu.edu.tw">npbsd1</option>
                                <option value="npbsd2.cs.nctu.edu.tw">npbsd2</option>
                                <option value="npbsd3.cs.nctu.edu.tw">npbsd3</option>
                                <option value="npbsd4.cs.nctu.edu.tw">npbsd4</option>
                                <option value="npbsd5.cs.nctu.edu.tw">npbsd5</option>
                            </select>
                            <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                            </div>
                        </div>
                    </td>
                    <td>
                        <input name="p1" type="text" class="form-control" size="5" />
                    </td>
                    <td>
                        <select name="f1" class="custom-select">
                            <option></option>
                            <option value="t1.txt">t1.txt</option>
                            <option value="t2.txt">t2.txt</option>
                            <option value="t3.txt">t3.txt</option>
                            <option value="t4.txt">t4.txt</option>
                            <option value="t5.txt">t5.txt</option>
                            <option value="t6.txt">t6.txt</option>
                            <option value="t7.txt">t7.txt</option>
                            <option value="t8.txt">t8.txt</option>
                            <option value="t9.txt">t9.txt</option>
                            <option value="t10.txt">t10.txt</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <th scope="row" class="align-middle">Session 3</th>
                    <td>
                        <div class="input-group">
                            <select name="h2" class="custom-select">
                                <option></option>
                                <option value="nplinux1.cs.nctu.edu.tw">nplinux1</option>
                                <option value="nplinux2.cs.nctu.edu.tw">nplinux2</option>
                                <option value="nplinux3.cs.nctu.edu.tw">nplinux3</option>
                                <option value="nplinux4.cs.nctu.edu.tw">nplinux4</option>
                                <option value="nplinux5.cs.nctu.edu.tw">nplinux5</option>
                                <option value="npbsd1.cs.nctu.edu.tw">npbsd1</option>
                                <option value="npbsd2.cs.nctu.edu.tw">npbsd2</option>
                                <option value="npbsd3.cs.nctu.edu.tw">npbsd3</option>
                                <option value="npbsd4.cs.nctu.edu.tw">npbsd4</option>
                                <option value="npbsd5.cs.nctu.edu.tw">npbsd5</option>
                            </select>
                            <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                            </div>
                        </div>
                    </td>
                    <td>
                        <input name="p2" type="text" class="form-control" size="5" />
                    </td>
                    <td>
                        <select name="f2" class="custom-select">
                            <option></option>
                            <option value="t1.txt">t1.txt</option>
                            <option value="t2.txt">t2.txt</option>
                            <option value="t3.txt">t3.txt</option>
                            <option value="t4.txt">t4.txt</option>
                            <option value="t5.txt">t5.txt</option>
                            <option value="t6.txt">t6.txt</option>
                            <option value="t7.txt">t7.txt</option>
                            <option value="t8.txt">t8.txt</option>
                            <option value="t9.txt">t9.txt</option>
                            <option value="t10.txt">t10.txt</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <th scope="row" class="align-middle">Session 4</th>
                    <td>
                        <div class="input-group">
                            <select name="h3" class="custom-select">
                                <option></option>
                                <option value="nplinux1.cs.nctu.edu.tw">nplinux1</option>
                                <option value="nplinux2.cs.nctu.edu.tw">nplinux2</option>
                                <option value="nplinux3.cs.nctu.edu.tw">nplinux3</option>
                                <option value="nplinux4.cs.nctu.edu.tw">nplinux4</option>
                                <option value="nplinux5.cs.nctu.edu.tw">nplinux5</option>
                                <option value="npbsd1.cs.nctu.edu.tw">npbsd1</option>
                                <option value="npbsd2.cs.nctu.edu.tw">npbsd2</option>
                                <option value="npbsd3.cs.nctu.edu.tw">npbsd3</option>
                                <option value="npbsd4.cs.nctu.edu.tw">npbsd4</option>
                                <option value="npbsd5.cs.nctu.edu.tw">npbsd5</option>
                            </select>
                            <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                            </div>
                        </div>
                    </td>
                    <td>
                        <input name="p3" type="text" class="form-control" size="5" />
                    </td>
                    <td>
                        <select name="f3" class="custom-select">
                            <option></option>
                            <option value="t1.txt">t1.txt</option>
                            <option value="t2.txt">t2.txt</option>
                            <option value="t3.txt">t3.txt</option>
                            <option value="t4.txt">t4.txt</option>
                            <option value="t5.txt">t5.txt</option>
                            <option value="t6.txt">t6.txt</option>
                            <option value="t7.txt">t7.txt</option>
                            <option value="t8.txt">t8.txt</option>
                            <option value="t9.txt">t9.txt</option>
                            <option value="t10.txt">t10.txt</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <th scope="row" class="align-middle">Session 5</th>
                    <td>
                        <div class="input-group">
                            <select name="h4" class="custom-select">
                                <option></option>
                                <option value="nplinux1.cs.nctu.edu.tw">nplinux1</option>
                                <option value="nplinux2.cs.nctu.edu.tw">nplinux2</option>
                                <option value="nplinux3.cs.nctu.edu.tw">nplinux3</option>
                                <option value="nplinux4.cs.nctu.edu.tw">nplinux4</option>
                                <option value="nplinux5.cs.nctu.edu.tw">nplinux5</option>
                                <option value="npbsd1.cs.nctu.edu.tw">npbsd1</option>
                                <option value="npbsd2.cs.nctu.edu.tw">npbsd2</option>
                                <option value="npbsd3.cs.nctu.edu.tw">npbsd3</option>
                                <option value="npbsd4.cs.nctu.edu.tw">npbsd4</option>
                                <option value="npbsd5.cs.nctu.edu.tw">npbsd5</option>
                            </select>
                            <div class="input-group-append">
                                <span class="input-group-text">.cs.nctu.edu.tw</span>
                            </div>
                        </div>
                    </td>
                    <td>
                        <input name="p4" type="text" class="form-control" size="5" />
                    </td>
                    <td>
                        <select name="f4" class="custom-select">
                            <option></option>
                            <option value="t1.txt">t1.txt</option>
                            <option value="t2.txt">t2.txt</option>
                            <option value="t3.txt">t3.txt</option>
                            <option value="t4.txt">t4.txt</option>
                            <option value="t5.txt">t5.txt</option>
                            <option value="t6.txt">t6.txt</option>
                            <option value="t7.txt">t7.txt</option>
                            <option value="t8.txt">t8.txt</option>
                            <option value="t9.txt">t9.txt</option>
                            <option value="t10.txt">t10.txt</option>
                        </select>
                    </td>
                </tr>
                <tr>
                    <td colspan="3"></td>
                    <td>
                        <button type="submit" class="btn btn-info btn-block">Run</button>
                    </td>
                </tr>
            </tbody>
        </table>
    </form>
</body>
</html>
        )";
		buf_ += "\r\n";
		auto self(shared_from_this());
		boost::asio::async_write(
			socket_, boost::asio::buffer(buf_),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				close();
			}
		});
	}

	void exec_welcome_cgi(const std::string &protocol) {
		buf_ = protocol + " 200 OK\r\n";
		buf_ += "Content-type: text/html\r\n\r\n";
		buf_ += "<h1>Welcome</h1>\r\n";
		auto self(shared_from_this());
		boost::asio::async_write(
			socket_, boost::asio::buffer(buf_),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec)
				close();
		});
	}

	class Client : public std::enable_shared_from_this<Client> {
	public:
		Client(std::shared_ptr<Session> session, std::string host, std::string port,
			std::string filename, std::string sname)
			: session_(session), resolv_(ioservice), socket_(ioservice),
			host_(host), port_(port), filename_(filename), sname_(sname) {}

		void connect() {
			if (filename_.empty())
				return;
			file_.open("test_case\\" + filename_);
			if (!file_.is_open())
				return;
			boost::asio::ip::tcp::resolver::query q{ host_, port_ };
			auto self(shared_from_this());
			resolv_.async_resolve(
				q, [this, self](const boost::system::error_code &ec,
					boost::asio::ip::tcp::resolver::iterator it) {
				if (!ec) {
					auto self(shared_from_this());
					socket_.async_connect(
						*it, [this, self](const boost::system::error_code &ec) {
						if (!ec) {
							receive();
						}
					});
				}
			});
		}

		void close() {
			file_.close();
			socket_.close();
			// server_socket_.close();
		}

	private:
		void receive() {
			auto self(shared_from_this());
			boost::asio::async_read_until(
				socket_, buffer_, boost::regex("^% "),
				[this, self](boost::system::error_code ec, size_t length) {
				if (!ec) {
					std::string res((std::istreambuf_iterator<char>(&buffer_)),
						std::istreambuf_iterator<char>());
					output_shell(res);
				}
				else {
					close();
				}
			});
		}

		void send() {
			auto self(shared_from_this());
			boost::asio::async_write(
				socket_, boost::asio::buffer(buf_),
				[this, self](boost::system::error_code ec, std::size_t length) {
				if (!ec)
					receive();
				else
					close();
			});
		}

		void output_shell(std::string rhs) {
			html_escape(rhs);
			buf_ = "<script>document.getElementById('" + sname_ +
				"').innerHTML += '" + rhs + "';</script>\r\n";
			auto self(shared_from_this());
			boost::asio::async_write(
				session_->socket_, boost::asio::buffer(buf_),
				[this, self](boost::system::error_code ec, std::size_t length) {
				if (!ec) {
					if (std::getline(file_, cmd_)) {
						cmd_ += '\n';
						output_command(cmd_);
					}
					else {
						close();
					}
				}
				else {
					close();
				}
			});
		}

		void output_command(std::string rhs) {
			html_escape(rhs);
			buf_ = "<script>document.getElementById('" + sname_ +
				"').innerHTML += '<b>" + rhs + "</b>';</script>\r\n";
			auto self(shared_from_this());
			boost::asio::async_write(
				session_->socket_, boost::asio::buffer(buf_),
				[this, self](boost::system::error_code ec, std::size_t length) {
				buf_ = cmd_;
				if (!ec)
					send();
				else
					close();
			});
		}

		static void html_escape(std::string &rhs) {
			std::string res;
			for (char c : rhs) {
				switch (c) {
				case '&':
					res.append("&amp;");
					break;
				case '\'':
					res.append("&quot;");
					break;
				case '\"':
					res.append("&apos;");
					break;
				case '<':
					res.append("&lt;");
					break;
				case '>':
					res.append("&gt;");
					break;
				case '\r':
					break;
				case '\n':
					res.append("&NewLine;");
					break;
				default:
					res += c;
					break;
				}
			}
			res.swap(rhs);
		}

		std::ifstream file_;
		std::string buf_, cmd_, host_, port_, filename_, sname_;
		tcp::resolver resolv_;
		tcp::socket socket_;
		boost::asio::streambuf buffer_;
		std::shared_ptr<Session> session_;
	};

	struct Query {
		std::string host, port, file, session;
	} query_[5];
	std::string buf_, req_file_, req_query_;
	tcp::socket socket_;
	boost::asio::streambuf buffer_;
};

class Server {
public:
	Server(unsigned short port)
		: acceptor_(ioservice, { tcp::v4(), port }, true), socket_(ioservice) {
		accept();
	}

private:
	void accept() {
		acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
			if (!ec) {
				std::make_shared<Session>(std::move(socket_))->start();
			}
			accept();
		});
	}

	tcp::acceptor acceptor_;
	tcp::socket socket_;
};

int main(int argc, char **argv) {
	try {
		if (argc < 2)
			throw std::logic_error("usage: cgi_server port");
		unsigned short port = std::atoi(argv[1]);
		Server server(port);
		ioservice.run();
	}
	catch (std::exception &e) {
		std::cerr << "[Server Error] " << e.what() << std::endl;
		exit(-1);
	}
	return 0;
}