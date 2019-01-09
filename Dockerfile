# This Dockerfile is for deployment, not for builds - it will not handle installing tsc or emcc.
FROM pypy:3-slim
WORKDIR /app
COPY requirements.txt /
RUN apt-get update && apt-get install -yq clang && pip install -r /requirements.txt && apt-get purge -yq clang && apt-get autoremove -yq
COPY . /
CMD ["pypy3", "/app.py"]