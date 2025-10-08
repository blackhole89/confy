# confy-setup { line: "#-", meta_line: "#!" }

all: cppexample

#! string $fname "Output binary name" = "hello";

#! template {
#-cppexample: cppstyle.cpp
#-	g++ -o $fname cppstyle.cpp
#! } into {
cppexample: cppstyle.cpp
	g++ -o hello cppstyle.cpp
#! }
