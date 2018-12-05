#!/bin/bash
while true
do
  cat README.md | pandoc -f markdown_github > readme.html
  sleep 1
done
