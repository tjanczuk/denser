an experiment with high density server side java script
===

**Prerequisites**

- Windows
- [Visual Studio C++ Express](http://www.microsoft.com/visualstudio/en-us/products/2010-editions/visual-cpp-express)

**Building**

`msbuild /t:Clean,Build /p:Configuration=Release src\denser.sln`

**Running**

First `cd build\release`, then:

run one chat application:

`denser.exe chat2.js`

run three chat applications in a single process using V8 isolates as an isolation mechanism:

`denser.exe chat2.js chat2.js chat2.js`

run fifty chat applications in a single process using V8 isolates

`denser.exe /config config.50.js`

run a thousand chat applications in a single process using V8 context as an isolation mechanism:

`denser.exe /useContext /config config.1000.js`