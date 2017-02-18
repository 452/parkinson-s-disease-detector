# Parkinson's disease detector

https://en.wikipedia.org/wiki/Parkinson's_disease

Hardware:
========

[Chip: MPU-9250 + BMP280](docs/hardware-specifications/MPU-9250)

Excell formulas:
========

Convert unix time to Excel:
```excel
=(((A2/60)/60)/24)+DATE(1970,1,1)
```

Basic filter by average data
```excel
=AVERAGE(A1:$A$259)
```

Exponential smoothing filter
alpha = 0.2
```excel
=alpha * dataForFilter + (1 - alpha) * previousCalculatedData
=$E$2 * $A$1 + (1 - $E$2) * $E$4
```

[GNU Octave](https://www.gnu.org/software/octave)
========
GNU Octave - Scientific Programming Language

Show temperature on the chart
```octave
pd = dlmread('pd.csv')
temperature = pd(1:end,1)
plot(temperature)
pkg load signal
plot(medfilt1(temperature))
plot(sgolayfilt(temperature))
```

Usage
-----
```sh
docker run -it -v $(pwd):/pd --rm --name pd i452/pd
docker run -it -v $(pwd):/pd --rm -p 9999:8080 --name pd i452/pd
# -p 9999:8080 http://localhost:9999/hawtio
```

Download Java libraries
```sh
docker run -it -v $(pwd):/pd -v $(pwd)/docker/libs:/root/.groovy/grapes --rm i452/groovy groovy pd.groovy
```

for compile code you need to create pd-sensor/WIFICredentials.h
```h
#define ROOT_PASSWORD_KIEV "777example"
#define OFFICE_PASSWORD_KIEV "30example"
```