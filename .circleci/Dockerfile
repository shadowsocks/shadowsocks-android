FROM circleci/android:api-28-ndk-r17b

RUN \
	wget -O /tmp/go.tgz "https://golang.org/dl/go1.11.linux-amd64.tar.gz"; \
	sudo tar -C /usr/local -xzf /tmp/go.tgz; \
	rm /tmp/go.tgz
ENV PATH /usr/local/go/bin:$PATH
