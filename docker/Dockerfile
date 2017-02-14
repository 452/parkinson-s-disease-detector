FROM i452/groovy
MAINTAINER emetemunoy <sp.titan@gmail.com>
RUN (echo 'alias g="groovy"' >> ~/.bashrc && \
	. ~/.bashrc)
COPY libs /root/.groovy/grapes
WORKDIR /pd
CMD ["/bin/bash"]