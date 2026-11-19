export async function acknowledgeRow(response,key,count){
 if(!response.ok)throw Error('Evidence HTTP '+response.status+': '+await response.text());
 const value=await response.json();
 if(value.saved!==true||value.key!==key||value.count!==count||!/^[0-9a-f]{64}$/.test(value.sha256??''))throw Error('invalid persistence ACK');
 return value;
}
export function verifyCompletion(index,expected){
 const keys=index.rows?.map(value=>value.key)??[];
 if(!index.complete||keys.length!==expected.length||new Set(keys).size!==keys.length||
    expected.some(key=>!keys.includes(key))||keys.some(key=>!expected.includes(key)))throw Error('missing/duplicate/unknown durable row');
 return index;
}
