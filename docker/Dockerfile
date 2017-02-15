FROM i452/groovy
MAINTAINER emetemunoy <sp.titan@gmail.com>
EXPOSE 8080
ENV HAWTIO_VERSION 1.4.68
RUN (apk update && \
apk add --no-cache openssl && \
wget -c https://oss.sonatype.org/content/repositories/public/io/hawt/hawtio-app/$HAWTIO_VERSION/hawtio-app-$HAWTIO_VERSION.jar -qO /opt/hawtio.jar && \
echo 'alias g="groovy"' >> ~/.bashrc && \
. ~/.bashrc)
COPY libs /root/.groovy/grapes
COPY jdk/tools.jar $JAVA_HOME/lib/
ADD run.sh /usr/local/bin/pd
ENV hawtio_config_cloneOnStartup false
WORKDIR /pd
ENTRYPOINT /usr/local/bin/pd