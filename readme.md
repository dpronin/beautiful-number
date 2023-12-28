examples of compiling (C++20 support is required):

> $ g++ task.cpp -O3 -std=gnu++20 -otask

> $ clang++ task.cpp -O3 -std=gnu++20 -otask -stdlib=libc++

> $ clang++ task.cpp -O3 -std=gnu++20 -otask -stdlib=libstdc++

example of running:

> $ ./task \<base\> \<order\>

example of running to meet the requirements of the task:

> $ ./task 13 13
