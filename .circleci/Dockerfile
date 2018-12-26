FROM circleci/android:api-28

# NDK see also: https://github.com/CircleCI-Public/circleci-dockerfiles/blob/cb8bda793023d3e919ea5553e2f2c04b71f53c49/android/images/api-28-ndk/Dockerfile#L181

ARG ndk_version=android-ndk-r19-beta2
ARG go_version=1.11
ARG android_ndk_home=/opt/android/${ndk_version}

# install NDK
RUN curl --silent --show-error --location --fail --retry 3 --output /tmp/${ndk_version}.zip https://dl.google.com/android/repository/${ndk_version}-linux-x86_64.zip && \
    sudo unzip -q /tmp/${ndk_version}.zip -d /opt/android && \
    rm /tmp/${ndk_version}.zip && \
    sudo chown -R circleci:circleci ${android_ndk_home}

ENV ANDROID_NDK_HOME ${android_ndk_home}

RUN \
    curl --silent --show-error --location --fail --retry 3 --output /tmp/go${go_version}.zip "https://golang.org/dl/go${go_version}.linux-amd64.tar.gz" && \
	sudo tar -C /usr/local -xzf /tmp/go${go_version}.tgz; \
	rm /tmp/go.tgz
ENV PATH /usr/local/go/bin:$PATH
