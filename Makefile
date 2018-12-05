.PHONY: test distfiles build

build:
	rm -rf backup-95b6895c8be94934de46237f.zip || true
	zip -r backup-95b6895c8be94934de46237f.zip \
	    server/content/*.py \
	    server/data/map.json \
	    server/*.py \
	    client/gfx \
	    client/map.data \
	    client/client \
	    client/LICENSE* \
	    client/README.txt \
	    thinclient/*.py \
	    thinclient/run.sh \
	    thinclient/static \
	    credits.txt
	@echo
	@echo Now upload this backup-95b6895c8be94934de46237f.zip file somewhere and
	@echo update the client/files/backup.sh file.
	mkdir -p resources
	cp backup-95b6895c8be94934de46237f.zip resources/
  

distfiles:
	mkdir -p distfiles
	cp -rp server distfiles/
	cp -rp client distfiles/
	cp -rp thinclient distfiles/

test:
	bash test.sh
	@echo 'Success!'
