import json, os, pathlib, subprocess, sys, tempfile
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    shader = root / 'controls.FX'
    (root / 'options.fxh').write_text('''
uniform float2 Tint < ui_label = "Tint colour"; ui_min = 0.0; ui_max = 1.0; > = float2(0.2, 0.8);
uniform int Mode < ui_type = "combo"; ui_items = "First\0Second\0"; > = 1;
uniform bool Enabled = true;
uniform float Zero;
uniform float Hidden < hidden = true; > = 1.0;
uniform float Time < source = "timer"; >;
''')
    shader.write_text('''#include "options.fxh"
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return float4(Tint, Mode + Zero, Enabled); }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
''')
    config = root / 'shader.conf'
    config.write_text(f'effects = A:B:makoDeband\nA = "{shader}"\nB = "{shader}"\n')
    def read():
        return json.loads(subprocess.check_output([sys.argv[1], str(config)], env={**os.environ,'VKBASALT_LOG_LEVEL':'none'}))['effects']
    first = read()
    assert len(first) == 3, first
    for effect in first: assert effect['error'] is None, effect
    parameters = first[0]['parameters']
    assert len(parameters) == 5, parameters
    assert parameters[0]['label'] == 'Tint colour [1]' and parameters[1]['label'] == 'Tint colour [2]'
    assert parameters[2]['items'] == ['First','Second']
    assert parameters[4]['default'] == 0
    assert len(first[2]['parameters']) == 5
    with config.open('a') as f: f.write(parameters[0]['key'] + ' = 0.6\n')
    changed = read()
    assert abs(changed[0]['parameters'][0]['value'] - .6) < 1e-6
    assert changed[0]['parameters'][1]['value'] == first[0]['parameters'][1]['value']
    assert changed[1]['parameters'] == first[1]['parameters']
    with config.open('a') as f: f.write(parameters[0]['key'] + ' = default\n')
    reset = read()
    assert reset[0]['parameters'][0]['value'] == first[0]['parameters'][0]['default']
    shader.write_text('invalid source')
    failed = read()
    assert failed[0]['error'] and failed[1]['error'] and not failed[2]['error']

    # Scalar arrays used to interpret literal values as component IDs and abort.
    # Cover initialized/zero scalar arrays alongside vector arrays and matrices.
    for declaration, expression, defaults in (
        ('uniform float Weights[2] = {0.25, 0.75};', 'Weights[0]', [0.25, 0.75]),
        ('uniform float Weights[2];', 'Weights[0]', [0, 0]),
        ('uniform float Weights[2] = {0.25, 0.75};', 'Weights[(uint)p.x % 2]', [0.25, 0.75]),
        ('uniform float Weights[2];', 'Weights[(uint)p.x % 2]', [0, 0]),
        ('uniform bool Weights[2] = {true, false};', 'Weights[(uint)p.x % 2] ? 1.0 : 0.0', [1, 0]),
        ('uniform int Weights[2] = {1, 2};', 'Weights[(uint)p.x % 2]', [1, 2]),
        ('uniform uint Weights[2] = {1, 2};', 'Weights[(uint)p.x % 2]', [1, 2]),
        ('uniform float2 Weights[2];', 'Weights[(uint)p.x % 2].x', [0, 0, 0, 0]),
        ('uniform float2 Weights[2] = {float2(0.25, 0.75), float2(0.5, 1)};', 'Weights[0].x', [0.25, 0.75, 0.5, 1]),
        ('uniform float2x2 Weights = float2x2(0.25, 0.75, 0.5, 1);', 'Weights[0][0]', [0.25, 0.75, 0.5, 1]),
    ):
        shader.write_text(declaration + """
void VS(uint id : SV_VertexID, out float4 p : SV_Position) { p = 0; }
float4 PS(float4 p : SV_Position) : SV_Target { return """ + expression + """; }
technique T { pass { VertexShader = VS; PixelShader = PS; } }
""")
        config.write_text(f'effects = A\nA = "{shader}"\n')
        effect = read()[0]
        assert effect['error'] is None, effect
        assert [p['default'] for p in effect['parameters']] == defaults, effect
        keys = [p['key'] for p in effect['parameters']]
        assert len(set(keys)) == len(defaults), effect
        value = 0.125 if effect['parameters'][-1]['type'] == 'float' else 1
        with config.open('a') as f: f.write(keys[-1] + f' = {value}\n')
        assert [p['value'] for p in read()[0]['parameters']] == defaults[:-1] + [value]
