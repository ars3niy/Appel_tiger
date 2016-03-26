for i in samples/*; do
    echo -n "$i "
    build/compiler $i && echo
done
