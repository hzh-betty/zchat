#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ca_dir="${repo_root}/server/certs/zchat-ca"
grpc_dir="${repo_root}/server/certs/grpc"
rabbitmq_dir="${repo_root}/server/certs/rabbitmq"
es_dir="${repo_root}/server/certs/elasticsearch"
etcd_dir="${repo_root}/server/certs/etcd"

mkdir -p "${ca_dir}" "${grpc_dir}" "${rabbitmq_dir}" "${es_dir}" "${etcd_dir}"

# ==========================================
# 统一 CA
# ==========================================
if [ ! -f "${ca_dir}/ca.pem" ]; then
    openssl genrsa -out "${ca_dir}/ca-key.pem" 2048
    openssl req -new -x509 -nodes -days 3650 \
        -key "${ca_dir}/ca-key.pem" \
        -sha256 \
        -subj "/CN=zchat-dev-ca" \
        -out "${ca_dir}/ca.pem"
    chmod 600 "${ca_dir}/ca-key.pem"
fi

# 通用函数：生成 server + client 证书
gen_cert() {
    local name="$1" dir="$2" cn="$3" san="$4"
    openssl genrsa -out "${dir}/${name}-key.pem" 2048
    openssl req -new \
        -key "${dir}/${name}-key.pem" \
        -subj "/CN=${cn}" \
        -out "${dir}/${name}.csr"
    cat >"${dir}/${name}-ext.cnf" <<EOF
subjectAltName=${san}
extendedKeyUsage=serverAuth,clientAuth
EOF
    openssl x509 -req \
        -in "${dir}/${name}.csr" \
        -CA "${ca_dir}/ca.pem" \
        -CAkey "${ca_dir}/ca-key.pem" \
        -CAcreateserial \
        -out "${dir}/${name}-cert.pem" \
        -days 3650 \
        -sha256 \
        -extfile "${dir}/${name}-ext.cnf"
    cp "${ca_dir}/ca.pem" "${dir}/ca.pem"
    chmod 644 "${dir}/${name}-key.pem"
}

# ==========================================
# gRPC（mTLS：服务端 + 客户端共用同一证书）
# ==========================================
gen_cert "server" "${grpc_dir}" "zchat-grpc" "DNS:localhost,DNS:file-service,DNS:speech-service,DNS:transmite-service,DNS:message-service,DNS:friend-service,DNS:user-service,DNS:gateway,IP:127.0.0.1"
gen_cert "client" "${grpc_dir}" "zchat-grpc-client" "DNS:localhost,IP:127.0.0.1"

# ==========================================
# RabbitMQ
# ==========================================
gen_cert "server" "${rabbitmq_dir}" "rabbitmq" "DNS:rabbitmq,DNS:localhost,IP:127.0.0.1"

# ==========================================
# Elasticsearch
# ==========================================
gen_cert "server" "${es_dir}" "elasticsearch" "DNS:elasticsearch,DNS:localhost,IP:127.0.0.1"

# ==========================================
# etcd
# ==========================================
gen_cert "server" "${etcd_dir}" "etcd" "DNS:etcd,DNS:localhost,IP:127.0.0.1"

echo "All certificates generated under server/certs/"
