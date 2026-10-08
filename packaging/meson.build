foreach manifest : ['vkBasalt.x86.json', 'vkBasalt.x86_64.json']
    configure_file(input: manifest + '.in', output: manifest,
                   configuration: {'api_version': vkbasalt_api_version})
endforeach
