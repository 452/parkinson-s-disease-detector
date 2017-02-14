# Parkinson's disease detector

https://en.wikipedia.org/wiki/Parkinson's_disease

Hardware:
========

Chip: MPU-9250 + BMP280

Excell formulas:
========

Convert unix time to Excell:
```excell
=(((A2/60)/60)/24)+DATE(1970,1,1)
```

Usage
-----
```sh
docker run -it -v $(pwd):/pd --rm i452/pd groovy pd.groovy
```

Download Java libraries
```sh
docker run -it -v $(pwd):/pd -v $(pwd)/docker/libs:/root/.groovy/grapes --rm i452/groovy groovy pd.groovy
```
