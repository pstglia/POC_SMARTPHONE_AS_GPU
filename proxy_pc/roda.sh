#!/bin/bash 

if [ $# -lt 1 ]
then
  echo "Uso: $0 IP"
  echo "Ex: $0 10.170.47.49"
  exit 255
fi

p_ip=${1}

# Define a pasta onde está o JSON local do usuário
export VK_LAYER_PATH=~/.local/share/vulkan/explicit_layer.d

# Ativa a nossa camada
export VK_INSTANCE_LAYERS=VK_LAYER_USB_SMARTPHONE_PROXY

# INJETA O IP DO TETHERING DINÂMICO DESTE MOMENTO
export SMARTPHONE_IP=${p_ip}

vkcube
