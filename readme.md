# request - Simple Http

```cpp
int main(int argc, char * argv[])
{
  requests::response r = requests::get(urls::url_view("https://httpbin.org/basic-auth/user/pass"),
                                       requests::headers({requests::basic_auth("user", "pass")}));

  std::cout << r.result_code() << std::endl;
  // 200
  std::cout << r.headers["Content-Type"] << std::endl;
  // 'application/json; charset=utf8'

  std::cout << r.string_view() << std::endl;
  // {"authenticated": true, ...

  std::cout << as_json(r) << std::endl;
  // {'authenticated': True, ...}
  return 0;
}
```

This C++14 library provides facilities to easily perform http requests, often with single function calls. 

This library is in alpha.

Read the docs [here](doc/requests.adoc).

## License

Distributed under the [Boost Software License, Version 1.0](http://boost.org/LICENSE_1_0.txt).
