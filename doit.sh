#!/usr/bin/env bash
set -euo pipefail

usb_uuid='2A81-1677'
mount_directory='/mnt/usb'
owner_user='devops'

printf 'Será desmontado e montado novamente o pendrive UUID %s em %s.\n' "$usb_uuid" "$mount_directory"
printf 'A montagem será de leitura/escrita, permitindo escrita ao usuário %s.\n' "$owner_user"
printf 'Nenhum arquivo será apagado e o dispositivo não será formatado.\n'
printf 'Pressione somente y minúsculo para continuar, ou qualquer outra tecla para cancelar: '
if [[ ! -t 0 ]]; then
    printf '\nCancelado: execute em um terminal interativo.\n'
    exit 0
fi
confirmation=''
IFS= read -r -n 1 confirmation || true
printf '\n'
if [[ "$confirmation" != 'y' ]]; then
    printf 'Operação cancelada.\n'
    exit 0
fi

fail() {
    printf 'Erro: %s\n' "$1" >&2
    exit 1
}

# O nome sdb1 pode mudar; o UUID identifica o volume escolhido nesta máquina.
device_alias="/dev/disk/by-uuid/$usb_uuid"
if ! device="$(readlink -f -- "$device_alias")" || [[ ! -b "$device" ]]; then
    fail "Pendrive esperado não encontrado ($device_alias)."
fi
[[ "$(lsblk -dn -o FSTYPE "$device")" == 'exfat' ]] || fail 'O volume esperado não é exFAT.'
lsblk -sn -o TRAN "$device" | grep -qx 'usb' || fail 'O dispositivo não foi identificado como USB.'
[[ -d "$mount_directory" && ! -L "$mount_directory" ]] || fail 'O ponto de montagem não existe ou é um link simbólico.'
owner_uid="$(id -u "$owner_user")"
owner_gid="$(id -g "$owner_user")"

mounted_source="$(findmnt -rn --mountpoint "$mount_directory" -o SOURCE || true)"
if [[ -n "$mounted_source" ]]; then
    [[ "$(readlink -f -- "$mounted_source")" == "$device" ]] || fail 'Outro volume está montado em /mnt/usb.'
else
    [[ -z "$(find "$mount_directory" -mindepth 1 -maxdepth 1 -print -quit)" ]] || fail 'O diretório /mnt/usb não está vazio.'
fi
other_targets="$(findmnt -rn -S "$device" -o TARGET || true)"
[[ -z "$other_targets" || "$other_targets" == "$mount_directory" ]] || fail 'O volume possui montagem em outro local.'

# A senha é solicitada pelo sudo, nunca armazenada neste arquivo.
sudo -v
if [[ -n "$mounted_source" ]]; then
    printf 'Desmontando %s...\n' "$mount_directory"
    sudo umount -- "$mount_directory"
fi
printf 'Montando %s com leitura/escrita para %s...\n' "$device" "$owner_user"
if ! sudo mount -o "rw,uid=$owner_uid,gid=$owner_gid,umask=022" -- "$device" "$mount_directory"; then
    fail 'A montagem falhou; confira o estado do pendrive antes de utilizá-lo.'
fi
sudo -u "$owner_user" test -w "$mount_directory" || fail 'O usuário ainda não tem acesso de escrita.'
findmnt --mountpoint "$mount_directory" -o SOURCE,TARGET,FSTYPE,OPTIONS
printf 'Concluído: %s pode escrever em %s.\n' "$owner_user" "$mount_directory"
