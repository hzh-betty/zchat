#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cert_dir="${repo_root}/server/certs/mysql"

mkdir -p "${cert_dir}"

openssl genrsa -out "${cert_dir}/ca-key.pem" 2048
openssl req -new -x509 -nodes -days 3650 \
  -key "${cert_dir}/ca-key.pem" \
  -sha256 \
  -subj "/CN=zchat-mysql-dev-ca" \
  -out "${cert_dir}/ca.pem"

openssl genrsa -out "${cert_dir}/server-key.pem" 2048
openssl req -new \
  -key "${cert_dir}/server-key.pem" \
  -subj "/CN=zchat-mysql" \
  -out "${cert_dir}/server.csr"

cat >"${cert_dir}/server-ext.cnf" <<EOF
subjectAltName=DNS:zchat-mysql
extendedKeyUsage=serverAuth
EOF

openssl x509 -req \
  -in "${cert_dir}/server.csr" \
  -CA "${cert_dir}/ca.pem" \
  -CAkey "${cert_dir}/ca-key.pem" \
  -CAcreateserial \
  -out "${cert_dir}/server-cert.pem" \
  -days 3650 \
  -sha256 \
  -extfile "${cert_dir}/server-ext.cnf"

chmod 600 "${cert_dir}/ca-key.pem"
chmod 644 "${cert_dir}/server-key.pem"
